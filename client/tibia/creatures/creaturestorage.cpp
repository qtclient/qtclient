#include "creatures/creaturestorage.h"

#include "appearances/appearanceinstance.h"
#include <tibia/config/runtimegameconfig.h>
#include "protocol/protocolcodes.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "tibia/creatures/attachedeffect.h"
#include "creatures/walkdebug.h"
#include "game/localplayersessionthing.h"
#include "worldmap/worldmapfield.h"
#include <tibia/game/gamesession.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <tibia/assets/things.h>

#include <algorithm>

#include <QString>

#include <algorithm>
#include <cmath>
#include <tibia/game/thingsession_forward.h>

namespace tibia::creatures {

namespace {

TCreatureStorage* g_creatureStorage = nullptr;

} // namespace

TCreature::TCreature(const TCreatureID id, QObject* parent)
    : cip::tibia::creatures::TCreature(parent)
    , m_creatureId(id)
{
}

ThingType* TCreature::outfitThingType() const
{
    if (!m_outfit.isCreature())
        return nullptr;
    return tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory());
}

ThingType* TCreature::mountThingType() const
{
    return m_outfit.hasMount() ? tibia::game::rawThingType(m_outfit.getMount(), ThingCategoryCreature) : nullptr;
}

namespace {

std::optional<animation::TMovingOutfitAnimator> walkAnimatorFor(const ThingType* type)
{
    const animation::TAnimationInfo* moving = type ? type->movingAnimation() : nullptr;
    if (!moving || moving->phaseCount() == 0)
        return std::nullopt;
    return animation::TMovingOutfitAnimator(moving->phaseCount(),
                                            moving->loopType == animation::TAnimationInfo::ELoopType::PingPong);
}

// A rebuilt sprite sequence keeps its walk animator, so the leg cycle runs on;
// the divisor becomes the plain phase count even for a ping-pong animation.
void refreshWalkAnimator(std::optional<animation::TMovingOutfitAnimator>& walk, const ThingType* type)
{
    const animation::TAnimationInfo* moving = type ? type->movingAnimation() : nullptr;
    if (walk && moving && moving->phaseCount() > 0)
        walk->setDivisor(moving->phaseCount());
    else
        walk = walkAnimatorFor(type);
}

}

static std::optional<animation::TAnimator> idleAnimatorFor(const ThingType* type)
{
    if (const animation::TAnimationInfo* info = type ? type->idleAnimation() : nullptr)
        return animation::TAnimator(*info, 0);
    return std::nullopt;
}

void TCreature::restartWalkAnimators()
{
    m_outfitWalkAnimator = walkAnimatorFor(outfitThingType());
    m_mountWalkAnimator = walkAnimatorFor(mountThingType());
    m_outfitIdleAnimator = idleAnimatorFor(tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory()));
    m_mountIdleAnimator = idleAnimatorFor(mountThingType());
}

void TCreature::refreshWalkAnimators()
{
    refreshWalkAnimator(m_outfitWalkAnimator, outfitThingType());
    refreshWalkAnimator(m_mountWalkAnimator, mountThingType());
    // A rebuilt standing sequence gets a new animator.
    m_outfitIdleAnimator = idleAnimatorFor(tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory()));
    m_mountIdleAnimator = idleAnimatorFor(mountThingType());
}

void TCreature::move(const qint64 startMs, const qint64 endMs, const worldmap::TWorldMapCoordinate& origin,
                     const EMovementDirection direction, const int elevationFrom, const int elevationTo)
{
    const auto look = lookDirectionAfter(static_cast<ECreatureLookDirection>(m_movementLook), direction);
    const bool lookChanged = static_cast<uint8_t>(look) != m_patternX;
    m_movementLook = static_cast<uint8_t>(look);
    const qint64 restartAfter = m_movementTween.endTime() + 1000;
    setDirection(static_cast<uint8_t>(look));
    // Keep the session camera Thing facing the step — otherwise idle
    // syncWorldDrawFrom used to be the only bridge and could race.
    if (m_isLocalPlayer) {
        if (const LocalPlayerSessionThingPtr local = tibia::game::localPlayer()) {
            if (local->getId() == m_creatureId)
                local->setDirection(static_cast<tibia::client::Direction>(m_patternX), false);
        }
    }

    const bool atOrigin = worldmap::TWorldMapCoordinate(m_position) == origin;
    const bool ended = m_movementTween.isFinished();
    const worldmap::TWorldMapExtent step = stepOffset(direction);
    const bool sameStep = m_movementTween.origin() == origin
        && m_movementTween.destination()
            == worldmap::TWorldMapCoordinate(origin.x + step.width, origin.y + step.height, origin.z + step.depth);

    bool started = false;
    if (!atOrigin) {
        if (!ended && sameStep && m_movementTween.mode() == 0) {
            m_movementTween.setMode(1);
        } else {
            m_movementTween.start(startMs, endMs, origin, direction, elevationFrom, elevationTo, 1);
            started = true;
        }
    } else if (ended) {
        m_movementTween.start(startMs, endMs, origin, direction, elevationFrom, elevationTo, 0);
        started = true;
    }
    if (started && (lookChanged || restartAfter < startMs))
        refreshWalkAnimators();
    if (m_outfitWalkAnimator)
        m_outfitWalkAnimator->setKey(endMs - startMs, startMs);
    if (m_mountWalkAnimator)
        m_mountWalkAnimator->setKey(endMs - startMs, startMs);
    if (lcWalk().isDebugEnabled()) {
        qCDebug(lcWalk) << "move" << "id" << m_creatureId << "local" << m_isLocalPlayer
                        << "origin" << origin.x << origin.y << origin.z << "dir" << int(direction)
                        << "pos" << walkPos(m_position) << "ms" << (endMs - startMs) << "mode"
                        << m_movementTween.mode() << "started" << started << "scale"
                        << m_movementTween.continueScale();
    }
    updateCoveredField();
}

void TCreature::resetMovement()
{
    m_movementTween = TCreatureMovementTween();
    m_suppressMovingSprite = true;
    refreshWalkAnimators();
    updateCoveredField();
}

void TCreature::update(const qint64 nowMs)
{
    m_movementTween.advance(nowMs);
    m_lastFrameMs = nowMs;
    if (isMoving()) {
        m_lastMovingFrameMs = nowMs;
        m_suppressMovingSprite = false;
    }
    if (m_outfitWalkAnimator)
        m_outfitWalkAnimator->update(nowMs);
    if (m_mountWalkAnimator)
        m_mountWalkAnimator->update(nowMs);
    if (m_outfitIdleAnimator)
        m_outfitIdleAnimator->update(nowMs);
    if (m_mountIdleAnimator)
        m_mountIdleAnimator->update(nowMs);
    m_animationPhase = animationPhaseFor(outfitThingType(), m_outfitWalkAnimator, m_outfitIdleAnimator);
    m_mountAnimationPhase = animationPhaseFor(mountThingType(), m_mountWalkAnimator, m_mountIdleAnimator);
    updateCoveredField();
    if (m_isLocalPlayer && walkDebugVerbose()) {
        const Point offset = walkOffset();
        qCDebug(lcWalk) << "frame" << nowMs << "pos" << walkPos(m_position) << "drawn"
                        << (m_position.x * 32 + offset.x) << (m_position.y * 32 + offset.y) << "moving" << isMoving()
                        << "phase" << m_animationPhase;
    }
}

uint16_t TCreature::animationPhaseFor(const ThingType* type,
                                      const std::optional<animation::TMovingOutfitAnimator>& walk,
                                      const std::optional<animation::TAnimator>& idle) const
{
    if (!type)
        return 0;
    if (showsMovingSprite() && walk)
        return static_cast<uint16_t>(type->getIdleAnimationPhases() + static_cast<int>(walk->phase()));
    return idle ? static_cast<uint16_t>(idle->phase()) : 0;
}

bool TCreature::showsMovingSprite() const
{
    if (!m_isLocalPlayer)
        return isMoving();
    const bool recentlyMoving = m_lastFrameMs <= m_lastMovingFrameMs + 75 && m_holdsMovementIntent;
    return (isMoving() || recentlyMoving) && !m_suppressMovingSprite;
}

int TCreature::drawElevation() const
{
    if (isMoving())
        return m_movementTween.elevation();
    return m_position.isValid() ? tibia::worldmap::TWorldMapStorage::ensureStorage()->elevationAt(m_position) : 0;
}

Position TCreature::coveredField() const
{
    const worldmap::TWorldMapExtent covered = m_movementTween.coveredFieldOffset();
    const worldmap::TWorldMapSubfieldExtent offset = m_movementTween.drawOffset();
    return Position(m_position.x + covered.width + (offset.width > 0 ? 1 : 0),
                    m_position.y + covered.height + (offset.height > 0 ? 1 : 0), m_position.z);
}

Point TCreature::walkOffset() const
{
    const worldmap::TWorldMapExtent covered = m_movementTween.coveredFieldOffset();
    const worldmap::TWorldMapSubfieldExtent offset = m_movementTween.drawOffset();
    return Point(covered.width * 32 + offset.width, covered.height * 32 + offset.height);
}

bool TCreature::isDisplaced() const
{
    const worldmap::TWorldMapExtent covered = m_movementTween.coveredFieldOffset();
    const worldmap::TWorldMapSubfieldExtent offset = m_movementTween.drawOffset();
    return covered.width != 0 || covered.height != 0 || offset.width != 0 || offset.height != 0;
}

void TCreature::updateCoveredField()
{
    const bool moving = isDisplaced() && m_position.isValid();
    const Position field = moving ? coveredField() : Position{};
    if (m_hasWalkingTile) {
        if (!moving || field != m_walkingTilePos) {
            if (tibia::worldmap::TWorldMapField* old = tibia::worldmap::fieldAt(m_walkingTilePos))
                old->removeWalkingCreature(m_creatureId);
            m_hasWalkingTile = false;
            m_walkingTilePos = {};
        } else if (tibia::worldmap::TWorldMapField* cur = tibia::worldmap::fieldAt(m_walkingTilePos)) {
            // MoveCreature scrubs walking slots before place; re-claim if still
            // on this covered field so the outfit does not vanish mid-step.
            cur->addWalkingCreature(m_creatureId);
        } else {
            m_hasWalkingTile = false;
            m_walkingTilePos = {};
        }
    }
    if (moving && !m_hasWalkingTile) {
        if (tibia::worldmap::TWorldMapField* covered = tibia::worldmap::getOrCreateField(field)) {
            covered->addWalkingCreature(m_creatureId);
            m_walkingTilePos = field;
            m_hasWalkingTile = true;
        }
    }
}

void TCreature::rebuildOutfitAppearance()
{
    ThingType* type = tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory());
    m_outfitAppearance = appearances::TAppearanceInstanceFactory::fromOutfit(m_outfit, type);
    if (const auto outfitInstance =
            std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(m_outfitAppearance))
        outfitInstance->setOutfitLook(m_outfit, m_patternX);
}

void TCreature::syncWorldDrawFrom(LocalPlayerSessionThing& player)
{
    if (!isDisplaced()) {
        m_position = player.getPosition();
        // Do not pull facing from the session Thing: move()/CancelWalk update
        // storage look, while session can lag until turn() runs. Copying session
        // here snapped the outfit back to an old cardinal after every step.
        const auto face = static_cast<tibia::client::Direction>(m_patternX);
        if (face <= tibia::client::West && player.getDirection() != face)
            player.setDirection(face, false);
    }
    m_isLocalPlayer = true;
    m_softDead = tibia::game::localPlayerIsSoftDead();
    tibia::game::syncLocalPlayerStorageRow(*this);
}

Color TCreature::informationColorForHealthPercent(const uint8_t healthPercent)
{
    static constexpr Color
        COLOR1(0x00, 0xBC, 0x00),
        COLOR2(0x50, 0xA1, 0x50),
        COLOR3(0xA1, 0xA1, 0x00),
        COLOR4(0xBF, 0x0A, 0x0A),
        COLOR5(0x91, 0x0F, 0x0F),
        COLOR6(0x85, 0x0C, 0x0C);

    if (healthPercent > 92)
        return COLOR1;
    if (healthPercent > 60)
        return COLOR2;
    if (healthPercent > 30)
        return COLOR3;
    if (healthPercent > 8)
        return COLOR4;
    if (healthPercent > 3)
        return COLOR5;
    return COLOR6;
}

void TCreature::setName(const QString& name)
{
    m_name = name;
}

void TCreature::setPosition(const Position& position)
{
    const bool floorChanged = m_position.z != position.z;
    m_position = position;
    if (floorChanged)
        resetMovement();
}

void TCreature::setCreatureType(const uint8_t creatureType)
{
    m_creatureType = creatureType;
}

void TCreature::setIsPlayer(const bool isPlayer)
{
    m_isPlayer = isPlayer;
}

void TCreature::setIsNpc(const bool isNpc)
{
    m_isNpc = isNpc;
}

void TCreature::setHidden(const bool hidden)
{
    m_hidden = hidden;
}

void TCreature::setHealthPercent(const uint8_t healthPercent)
{
    if (m_healthPercent == healthPercent)
        return;
    m_healthPercent = healthPercent;
    m_informationColor = informationColorForHealthPercent(healthPercent);
    m_dead = healthPercent == 0;
}

void TCreature::setOutfit(const Outfit& outfit)
{
    if (m_outfit == outfit)
        return;
    m_outfit = outfit;
    rebuildOutfitAppearance();
    restartWalkAnimators();
}

void TCreature::setLight(const Light& light)
{
    m_light = light;
}

void TCreature::setSpeed(const uint16_t speed)
{
    m_speed = speed;
}

void TCreature::setSkull(const uint8_t skull)
{
    m_skull = skull;
}

void TCreature::setShield(const uint8_t shield)
{
    m_shield = shield;
}

void TCreature::setEmblem(const uint8_t emblem)
{
    m_emblem = emblem;
}

void TCreature::setPassable(const bool passable)
{
    m_passable = passable;
}

namespace {

uint8_t patternXFromDirection(const uint8_t direction)
{
    // Match Creature::setDirection: diagonals collapse to East/West sprite patterns.
    if (direction == tibia::client::NorthEast || direction == tibia::client::SouthEast)
        return tibia::client::East;
    if (direction == tibia::client::NorthWest || direction == tibia::client::SouthWest)
        return tibia::client::West;
    if (direction <= tibia::client::West)
        return direction;
    return tibia::client::South;
}

} // namespace

void TCreature::setDirection(const uint8_t direction)
{
    if (direction == static_cast<uint8_t>(tibia::client::InvalidDirection))
        return;

    const uint8_t patternX = patternXFromDirection(direction);
    if (m_direction == direction && m_patternX == patternX)
        return;

    m_direction = direction;
    m_patternX = patternX;
    if (m_outfitAppearance) {
        if (const auto outfitInstance =
                std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(m_outfitAppearance))
            outfitInstance->setOutfitLook(m_outfit, m_patternX);
    }
}

void TCreature::setEchoRaidColor(const int8_t color)
{
    m_echoRaidColor = color;
}

void TCreature::setIcons(const std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& icons)
{
    m_hudIcons = icons;
}

void TCreature::setVocation(const uint8_t vocation)
{
    m_vocation = vocation;
}

void TCreature::attachEffect(const AttachedEffectPtr& effect)
{
    if (!effect)
        return;
    m_attachedEffects.push_back(effect);
}

void TCreature::detachEffectById(const uint16_t effectId)
{
    m_attachedEffects.erase(std::remove_if(m_attachedEffects.begin(), m_attachedEffects.end(),
                                           [effectId](const AttachedEffectPtr& e) {
                                               return e && e->getId() == effectId;
                                           }),
                            m_attachedEffects.end());
}

void TCreature::clearTemporaryAttachedEffects()
{
    m_attachedEffects.erase(std::remove_if(m_attachedEffects.begin(), m_attachedEffects.end(),
                                           [](const AttachedEffectPtr& e) {
                                               return e && !e->isPermanent();
                                           }),
                            m_attachedEffects.end());
}

void TCreature::setIcon(const uint8_t icon)
{
    m_icon = icon;
}

void TCreature::setInspectionType(const uint8_t inspectionType)
{
    m_inspectionType = inspectionType;
}

void TCreature::showStaticSquare(const Color& color)
{
    m_showStaticSquare = true;
    m_staticSquareColor = color;
}

void TCreature::hideStaticSquare()
{
    m_showStaticSquare = false;
}

void TCreature::addTimedSquare(const uint8_t color8bit)
{
    m_showTimedSquare = true;
    m_timedSquareColor = Color::from8bit(color8bit != 0 ? color8bit : 1);
    const TCreatureID id = m_creatureId;
    tibia::schedule::after([id] {
        if (TCreatureStorage* storage = TCreatureStorage::instance()) {
            if (const auto row = storage->creatureAt(id)) {
                row->removeTimedSquare();
                storage->publishCreatureUpdated(row);
            }
        }
    }, tibia::config::volatileSquareDuration());
}

void TCreature::removeTimedSquare()
{
    m_showTimedSquare = false;
}

TCreatureStorage::TCreatureStorage(QObject* parent)
    : cip::tibia::creatures::TCreatureStorage(parent)
{
}

TCreatureStorage::~TCreatureStorage()
{
    if (g_creatureStorage == this)
        g_creatureStorage = nullptr;
}

TCreatureStorage* TCreatureStorage::instance()
{
    return g_creatureStorage;
}

void TCreatureStorage::setInstance(TCreatureStorage* storage)
{
    g_creatureStorage = storage;
}

std::shared_ptr<TCreature> TCreatureStorage::getOrCreate(const TCreatureID id)
{
    const auto it = m_creatures.find(id);
    if (it != m_creatures.end())
        return it.value();

    auto created = std::make_shared<TCreature>(id);
    m_creatures.insert(id, created);
    return created;
}

std::shared_ptr<TCreature> TCreatureStorage::creatureAt(const TCreatureID id) const
{
    return m_creatures.value(id);
}

std::vector<std::shared_ptr<TCreature>> TCreatureStorage::spectatorsNear(const Position& center,
                                                                         const bool multiFloor,
                                                                         const int left, const int right,
                                                                         const int top, const int bottom) const
{
    // Match Map::getSpectatorsInRangeEx tile walk: inclusive [center - min, center + max].
    int minZ = center.z;
    int maxZ = center.z;
    if (multiFloor) {
        minZ = tibia::game::firstAwareFloor();
        maxZ = tibia::game::lastAwareFloor();
    }

    const int startX = center.x - left;
    const int endX = center.x + right;
    const int startY = center.y - top;
    const int endY = center.y + bottom;

    std::vector<std::shared_ptr<TCreature>> result;
    result.reserve(static_cast<size_t>(m_creatures.size()));
    for (const std::shared_ptr<TCreature>& creature : m_creatures) {
        if (!creature)
            continue;
        const Position& pos = creature->position();
        if (pos.z < minZ || pos.z > maxZ)
            continue;
        if (pos.x < startX || pos.x > endX || pos.y < startY || pos.y > endY)
            continue;
        result.push_back(creature);
    }
    return result;
}

void TCreatureStorage::remove(const TCreatureID id)
{
    if (!m_creatures.remove(id))
        return;
    Q_EMIT creatureRemoved(id);
}

void TCreatureStorage::leaveMap(const TCreatureID id)
{
    if (id == 0)
        return;
    if (const uint32_t localId = tibia::game::localPlayerId(); localId != 0 && localId == id) {
        // Own character stays in storage for the whole session (name/HUD).
        return;
    }
    const auto row = creatureAt(id);
    if (!row)
        return;
    // Drop map presence without forgetting KnownCreature identity.
    row->setPosition({});
    publishCreatureUpdated(row);
}

QList<TCreatureID> TCreatureStorage::creatureIds() const
{
    return m_creatures.keys();
}

void TCreatureStorage::syncWorldDrawFrom(LocalPlayerSessionThing& player)
{
    const TCreatureID id = player.getId();
    if (id == 0)
        return;
    getOrCreate(id)->syncWorldDrawFrom(player);
}

void TCreatureStorage::syncLocalPlayerStatsFromMap()
{
    tibia::game::syncLocalPlayerStorageWorldDraw();
}

void TCreatureStorage::publishCreatureUpdated(const std::shared_ptr<TCreature>& row,
                                            const ECreatureUpdateAppearanceType appearanceType)
{
    if (!row)
        return;
    const auto weak = std::weak_ptr<TCreature>(row);
    Q_EMIT creatureUpdated(weak);
    if (appearanceType != ECreatureUpdateAppearanceType::Health)
        Q_EMIT creatureAppearanceUpdated(weak, appearanceType);
}

std::shared_ptr<TCreature> TCreatureStorage::ensureFromProtocol(const TCreatureID id,
                                                               const QString& name,
                                                               const Outfit& outfit,
                                                               const uint8_t direction,
                                                               const uint8_t creatureType,
                                                               const bool isPlayer,
                                                               const bool isNpc,
                                                               const Position* position)
{
    if (id == 0)
        return nullptr;

    const bool isNew = !m_creatures.contains(id);
    const Outfit previousOutfit = isNew ? Outfit{} : m_creatures.value(id)->outfit();
    const bool outfitChanged = outfit.getId() != 0 && (isNew || previousOutfit != outfit);

    const auto row = getOrCreate(id);
    if (!name.isEmpty()) {
        row->setName(name);
    } else if (row->name().isEmpty()) {
        // KnownCreature / OutdatedCreature omit the name. CIP keeps the prior
        // KnownCreature row; if we just (re)created the LocalPlayerSessionThing slot, recover
        // the session character name so the nameplate does not go blank.
        if (const uint32_t localId = tibia::game::localPlayerId();
            localId == id && localId != 0) {
            const std::string name = tibia::game::localPlayerName();
            if (!name.empty())
                row->setName(QString::fromStdString(name));
        }
    }
    if (outfit.getId() != 0)
        row->setOutfit(outfit);
    row->setDirection(direction);
    if (position)
        row->setPosition(*position);
    if (creatureType != 0 || isPlayer || isNpc) {
        row->setCreatureType(creatureType);
        row->setIsPlayer(isPlayer);
        row->setIsNpc(isNpc);
        row->setHidden(creatureType == Proto::CreatureTypeHidden);
    }

    const auto weak = std::weak_ptr<TCreature>(row);
    Q_EMIT creatureUpdated(weak);
    if (isPlayer && isNew)
        Q_EMIT playerAdded(weak);
    if (outfitChanged)
        Q_EMIT creatureAppearanceUpdated(weak, ECreatureUpdateAppearanceType::FullOutfit);

    return row;
}

namespace {

void syncFieldStackFor(const TCreatureID id)
{
    if (id == 0)
        return;
    TCreatureStorage* storage = TCreatureStorage::instance();
    const auto row = storage ? storage->creatureAt(id) : nullptr;
    if (!row || !row->position().isValid())
        return;
    if (tibia::worldmap::TWorldMapField* field = tibia::worldmap::fieldAt(row->position()))
        field->syncCreatureStackAppearance(id);
}

} // namespace

std::shared_ptr<TCreature> ensureFromProtocol(const TCreatureID id,
                                              const QString& name,
                                              const Outfit& outfit,
                                              const uint8_t direction,
                                              const uint8_t creatureType,
                                              const bool isPlayer,
                                              const bool isNpc,
                                              const Position* position)
{
    if (TCreatureStorage* storage = TCreatureStorage::instance()) {
        return storage->ensureFromProtocol(id, name, outfit, direction, creatureType, isPlayer,
                                           isNpc, position);
    }
    return nullptr;
}

void protocolSetHealthPercent(const TCreatureID id, const uint8_t healthPercent)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setHealthPercent(healthPercent);
    storage->publishCreatureUpdated(row, ECreatureUpdateAppearanceType::Health);
}

void protocolSetLight(const TCreatureID id, const Light& light)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setLight(light);
    storage->publishCreatureUpdated(row);
}

void protocolSetOutfit(const TCreatureID id, const Outfit& outfit)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    const bool outfitChanged = row->outfit() != outfit;
    row->setOutfit(outfit);
    storage->publishCreatureUpdated(row, outfitChanged ? ECreatureUpdateAppearanceType::FullOutfit
                                                       : ECreatureUpdateAppearanceType::Health);
    syncFieldStackFor(id);
}

void protocolSetSpeed(const TCreatureID id, const uint16_t speed, const uint16_t /*baseSpeed*/)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setSpeed(speed);
    storage->publishCreatureUpdated(row);
}

void protocolSetSkull(const TCreatureID id, const uint8_t skull)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setSkull(skull);
    storage->publishCreatureUpdated(row);
}

void protocolSetShield(const TCreatureID id, const uint8_t shield)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setShield(shield);
    storage->publishCreatureUpdated(row);
}

void protocolSetPassable(const TCreatureID id, const bool passable)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setPassable(passable);
    storage->publishCreatureUpdated(row);
}

void protocolSetDirection(const TCreatureID id, const uint8_t direction)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setDirection(direction);
    storage->publishCreatureUpdated(row, ECreatureUpdateAppearanceType::Direction);
    syncFieldStackFor(id);
}

void protocolSetEchoRaidColor(const TCreatureID id, const int8_t color)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setEchoRaidColor(color);
    storage->publishCreatureUpdated(row);
}

void protocolSetIcons(const TCreatureID id, const std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& icons)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setIcons(icons);
    storage->publishCreatureUpdated(row);
}

void protocolSetVocation(const TCreatureID id, const uint8_t vocation)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setVocation(vocation);
    storage->publishCreatureUpdated(row);
}

void protocolAttachEffect(const TCreatureID id, const AttachedEffectPtr& effect)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage || !effect)
        return;
    const auto row = storage->getOrCreate(id);
    row->attachEffect(effect);
    storage->publishCreatureUpdated(row);
}

void protocolDetachEffect(const TCreatureID id, const uint16_t effectId)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->detachEffectById(effectId);
    storage->publishCreatureUpdated(row);
}

void protocolClearTemporaryAttachedEffects(const TCreatureID id)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->clearTemporaryAttachedEffects();
    storage->publishCreatureUpdated(row);
}

void protocolSetIcon(const TCreatureID id, const uint8_t icon)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setIcon(icon);
    storage->publishCreatureUpdated(row);
}

void protocolSetInspectionType(const TCreatureID id, const uint8_t inspectionType)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setInspectionType(inspectionType);
    storage->publishCreatureUpdated(row);
}

void protocolShowStaticSquare(const TCreatureID id, const Color& color)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->showStaticSquare(color);
    storage->publishCreatureUpdated(row);
}

void protocolHideStaticSquare(const TCreatureID id)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->hideStaticSquare();
    storage->publishCreatureUpdated(row);
}

void protocolAddTimedSquare(const TCreatureID id, const uint8_t color8bit)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->addTimedSquare(color8bit);
    storage->publishCreatureUpdated(row);
}

void protocolRemoveTimedSquare(const TCreatureID id)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->removeTimedSquare();
    storage->publishCreatureUpdated(row);
}

} // namespace tibia::creatures
