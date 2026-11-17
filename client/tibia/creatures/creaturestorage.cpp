#include "creatures/creaturestorage.h"

#include "appearances/appearanceinstance.h"
#include <tibia/config/runtimegameconfig.h>
#include "protocol/protocolcodes.h"
#include "clockservice.h"
#include "tibia/creatures/attachedeffect.h"
#include "creatures/walkdebug.h"
#include "game/playerdata.h"
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

/// Every creature made (or renewed) takes the next identity.
int g_nextCreatureUniqueId = 0;

TCreatureMarkOptions g_markOptions;

/// The frame colours: red for the attack target, green for the follow
/// target, lighter while also selected, white for a selected creature.
const QColor& selectedFrameColor()
{
    static const QColor color(Qt::white);
    return color;
}

const QColor& attackFrameColor()
{
    static const QColor color(Qt::red);
    return color;
}

const QColor& selectedAttackFrameColor()
{
    static const QColor color = QColor::fromRgb(0xff, 0x80, 0x80, 0xff);
    return color;
}

const QColor& followFrameColor()
{
    static const QColor color(Qt::green);
    return color;
}

const QColor& selectedFollowFrameColor()
{
    static const QColor color = QColor::fromRgb(0x80, 0xff, 0x80, 0xff);
    return color;
}

} // namespace

TCreature::TCreature(const TCreatureID id, QObject* parent)
    : cip::tibia::creatures::TCreature(parent)
    , m_creatureId(id)
    , m_uniqueId(g_nextCreatureUniqueId++)
{
}

void TCreature::renewUniqueId()
{
    m_uniqueId = g_nextCreatureUniqueId++;
}

const QString& TCreature::name() const
{
    return isPlayer() ? m_name : m_formattedName;
}

bool TCreature::isPlayer() const
{
    return m_creatureType == Proto::CreatureTypePlayer;
}

bool TCreature::isNpc() const
{
    return m_creatureType == Proto::CreatureTypeNpc;
}

bool TCreature::isHidden() const
{
    return m_creatureType == Proto::CreatureTypeHidden;
}

TAppearanceType* TCreature::outfitThingType() const
{
    if (!m_outfit.isCreature())
        return nullptr;
    return tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory());
}

TAppearanceType* TCreature::mountThingType() const
{
    return m_outfit.hasMount() ? tibia::game::rawThingType(m_outfit.getMount(), ThingCategoryCreature) : nullptr;
}

namespace {

std::optional<animation::TMovingOutfitAnimator> walkAnimatorFor(const TAppearanceType* type)
{
    const animation::TAnimationInfo* moving = type ? type->movingAnimation() : nullptr;
    if (!moving || moving->phaseCount() == 0)
        return std::nullopt;
    return animation::TMovingOutfitAnimator(moving->phaseCount(),
                                            moving->loopType == animation::TAnimationInfo::ELoopType::PingPong);
}

// A rebuilt sprite sequence keeps its walk animator, so the leg cycle runs on;
// the divisor becomes the plain phase count even for a ping-pong animation.
void refreshWalkAnimator(std::optional<animation::TMovingOutfitAnimator>& walk, const TAppearanceType* type)
{
    const animation::TAnimationInfo* moving = type ? type->movingAnimation() : nullptr;
    if (walk && moving && moving->phaseCount() > 0)
        walk->setDivisor(moving->phaseCount());
    else
        walk = walkAnimatorFor(type);
}

}

static std::optional<animation::TAnimator> idleAnimatorFor(const TAppearanceType* type)
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
    m_marks.timedSquareShown = nowMs < m_marks.timedSquareEndMs;
    updateCoveredField();
    if (m_isLocalPlayer && walkDebugVerbose()) {
        const QPoint offset = walkOffset();
        qCDebug(lcWalk) << "frame" << nowMs << "pos" << walkPos(m_position) << "drawn"
                        << (m_position.x * 32 + offset.x()) << (m_position.y * 32 + offset.y()) << "moving" << isMoving()
                        << "phase" << m_animationPhase;
    }
}

uint16_t TCreature::animationPhaseFor(const TAppearanceType* type,
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

QPoint TCreature::walkOffset() const
{
    const worldmap::TWorldMapExtent covered = m_movementTween.coveredFieldOffset();
    const worldmap::TWorldMapSubfieldExtent offset = m_movementTween.drawOffset();
    return QPoint(covered.width * 32 + offset.width, covered.height * 32 + offset.height);
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
    TAppearanceType* type = tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory());
    m_outfitAppearance = appearances::TAppearanceInstanceFactory::fromOutfit(m_outfit, type);
    if (m_outfitAppearance)
        m_outfitAppearance->linkCreature(m_creatureId);
    if (const auto outfitInstance =
            std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(m_outfitAppearance))
        outfitInstance->setOutfitLook(m_outfit, m_patternX);
}

void TCreature::syncLocalPlayerStats()
{
    m_isLocalPlayer = true;
    if (tibia::game::TPlayerData* data = tibia::game::TPlayerData::liveInstance()) {
        m_mana = data->mana();
        m_maxMana = data->maxMana();
        m_manaShield = data->manaShield();
        m_maxManaShield = data->maxManaShield();
        m_harmony = data->harmony();
        m_serene = data->serene();
        m_vocation = data->vocation();
        if (!data->characterName().empty())
            m_name = QString::fromStdString(data->characterName());
    }
}

void TCreature::setSoftDead(const bool softDead)
{
    if (m_softDead == softDead)
        return;
    m_softDead = softDead;
    if (softDead)
        m_healthPercent = 0;
}

void TCreature::setInformationChannel(const uint32_t durationMs, const qint64 nowMs)
{
    m_informationChannelStartMs = nowMs;
    m_informationChannelDurationMs = std::max<uint32_t>(durationMs, 1);
}

float TCreature::informationChannelProgress(const qint64 nowMs) const
{
    if (m_informationChannelDurationMs == 0)
        return -1.f;
    const auto elapsed = static_cast<float>(nowMs - m_informationChannelStartMs);
    return std::min(100.f, elapsed * 100.f / static_cast<float>(m_informationChannelDurationMs));
}

QColor TCreature::informationColorForHealthPercent(const uint8_t healthPercent)
{
    static const QColor
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
    m_formattedName = QString::fromStdString(tibia::game::formatCreatureName(name.toStdString()));
}

void TCreature::setPosition(const Position& position)
{
    const bool floorChanged = m_position.z != position.z;
    m_position = position;
    if (floorChanged)
        resetMovement();
    Q_EMIT positionWasUpdated(worldmap::TWorldMapCoordinate(m_position));
}

void TCreature::setOutfit(const Outfit& outfit)
{
    if (m_outfit == outfit)
        return;
    m_outfit = outfit;
    rebuildOutfitAppearance();
    restartWalkAnimators();
}

void TCreature::setSpeed(const uint16_t speed)
{
    m_speed = speed;
}

void TCreature::setSkull(const uint8_t skull)
{
    if (m_skull == skull)
        return;
    m_skull = skull;
    Q_EMIT playerKillerMarkChanged();
}

void TCreature::setShield(const uint8_t shield)
{
    if (m_shield == shield)
        return;
    m_shield = shield;
    Q_EMIT playerPartyFlagChanged();
}

void TCreature::setEmblem(const uint8_t emblem)
{
    if (m_emblem == emblem)
        return;
    m_emblem = emblem;
    Q_EMIT playerGuildFlagChanged();
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

void TCreature::setInspectionType(const uint8_t inspectionType)
{
    if (m_inspectionType == inspectionType)
        return;
    m_inspectionType = inspectionType;
    Q_EMIT inspectionStateChanged();
}

void TCreature::setTargetFrame(const QColor& color)
{
    m_marks.targetFrame = color;
}

void TCreature::hideTargetFrame()
{
    m_marks.targetFrame.setAlpha(0);
}

void TCreature::showStaticSquare(const QColor& color)
{
    m_marks.staticSquare = color;
}

void TCreature::hideStaticSquare()
{
    m_marks.staticSquare.setAlpha(0);
}

void TCreature::addTimedSquare(const QColor& color, const qint64 nowMs)
{
    m_marks.timedSquare = color;
    m_marks.timedSquareEndMs = nowMs + tibia::config::volatileSquareDuration();
    m_marks.timedSquareShown = true;
}

bool TCreature::hasMarks() const
{
    if (g_markOptions.targetFrame && m_marks.targetFrame.alpha() > 0)
        return true;
    if (g_markOptions.staticSquare && m_marks.staticSquare.alpha() > 0)
        return true;
    return m_marks.timedSquareShown && g_markOptions.timedSquare;
}

namespace {

std::vector<QColor> marksOf(const TCreatureMarks& marks, const bool withoutSelectionFrame)
{
    std::vector<QColor> colors;
    if (g_markOptions.targetFrame && marks.targetFrame.alpha() > 0
        && (!withoutSelectionFrame || marks.targetFrame != selectedFrameColor()))
        colors.push_back(marks.targetFrame);
    if (g_markOptions.staticSquare && marks.staticSquare.alpha() > 0)
        colors.push_back(marks.staticSquare);
    if (g_markOptions.timedSquare && marks.timedSquareShown)
        colors.push_back(marks.timedSquare);
    return colors;
}

}

std::vector<QColor> TCreature::marks() const
{
    return marksOf(m_marks, false);
}

std::vector<QColor> TCreature::shownMarks() const
{
    return marksOf(m_marks, g_markOptions.mouseHighlightTarget);
}

const TCreatureMarkOptions& TCreature::markOptions()
{
    return g_markOptions;
}

void TCreature::setMarkOptions(const TCreatureMarkOptions& options)
{
    g_markOptions = options;
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

std::shared_ptr<TCreature> TCreatureStorage::createCreature(const TCreatureID id)
{
    auto creature = std::make_shared<TCreature>(id);
    add(creature);
    return creature;
}

void TCreatureStorage::add(const std::shared_ptr<TCreature>& creature)
{
    const TCreatureID id = creature->creatureId();
    m_creatures.insert(id, creature);
    if (id == m_playerID)
        creature->markAsLocalPlayer();
    if (id == m_followID)
        setFollowID(id);
    else if (id == m_attackID)
        setAttackID(id);
}

void TCreatureStorage::setPlayerID(const TCreatureID id)
{
    m_playerID = id;
}

TCreatureID TCreatureStorage::playerID() const
{
    return m_playerID;
}

void TCreatureStorage::setTemporarySelectedID(const TCreatureID id)
{
    // The previous one keeps only its attack or follow frame.
    if (const auto previous = creatureAt(m_temporarySelectedID)) {
        previous->hideTargetFrame();
        if (m_temporarySelectedID == m_attackID)
            previous->setTargetFrame(attackFrameColor());
        else if (m_temporarySelectedID == m_followID)
            previous->setTargetFrame(followFrameColor());
    }
    const auto selected = creatureAt(id);
    if (!selected) {
        m_temporarySelectedID = 0;
        return;
    }
    m_temporarySelectedID = id;
    const QColor* color = m_followID == id ? &selectedFollowFrameColor() : &selectedFrameColor();
    if (m_attackID == id)
        color = &selectedAttackFrameColor();
    selected->setTargetFrame(*color);
}

void TCreatureStorage::setFollowID(const TCreatureID id)
{
    // Stopping keeps the white frame on a selected creature.
    if (const auto previous = creatureAt(m_followID)) {
        if (id == 0 && m_followID == m_temporarySelectedID)
            previous->setTargetFrame(selectedFrameColor());
        else
            previous->hideTargetFrame();
    }
    if (const auto attacked = creatureAt(m_attackID))
        attacked->hideTargetFrame();
    m_attackID = 0;
    const auto followed = creatureAt(id);
    if (!followed) {
        m_followID = 0;
        return;
    }
    m_followID = id;
    followed->setTargetFrame(m_temporarySelectedID == id ? selectedFollowFrameColor() : followFrameColor());
}

void TCreatureStorage::setAttackID(const TCreatureID id)
{
    if (const auto previous = creatureAt(m_attackID)) {
        if (id == 0 && m_attackID == m_temporarySelectedID)
            previous->setTargetFrame(selectedFrameColor());
        else
            previous->hideTargetFrame();
    }
    if (const auto followed = creatureAt(m_followID))
        followed->hideTargetFrame();
    m_followID = 0;
    const auto attacked = creatureAt(id);
    if (!attacked) {
        m_attackID = 0;
        return;
    }
    m_attackID = id;
    attacked->setTargetFrame(m_temporarySelectedID == id ? selectedAttackFrameColor() : attackFrameColor());
}

void TCreatureStorage::placeOnField(const TCreatureID id)
{
    if (const auto creature = creatureAt(id))
        creature->setPlacedOnField(true);
}

void TCreatureStorage::removeFromField(const TCreatureID id)
{
    if (const auto creature = creatureAt(id))
        creature->setPlacedOnField(false);
}

void TCreatureStorage::clearPlacedOnFieldFlags()
{
    for (const std::shared_ptr<TCreature>& creature : std::as_const(m_creatures))
        creature->setPlacedOnField(false);
}

std::shared_ptr<TCreature> TCreatureStorage::player() const
{
    return creatureAt(playerID());
}

std::vector<std::shared_ptr<TCreature>> TCreatureStorage::spectatorsNear(const Position& center,
                                                                         const bool multiFloor,
                                                                         const int left, const int right,
                                                                         const int top, const int bottom) const
{
    // The rectangle is inclusive: [center - left, center + right] and so on.
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

void TCreatureStorage::syncLocalPlayerStatsFromMap()
{
    if (const std::shared_ptr<TCreature> row = player()) {
        row->syncLocalPlayerStats();
        publishCreatureUpdated(row);
    }
}

void TCreatureStorage::notifyPlayerAdded(const std::shared_ptr<TCreature>& player)
{
    Q_EMIT playerAdded(player);
}

void TCreatureStorage::notifyCreatureUpdated(const std::shared_ptr<TCreature>& creature)
{
    Q_EMIT creatureUpdated(creature);
}

void TCreatureStorage::notifyCreatureAppearanceUpdated(const std::shared_ptr<TCreature>& creature,
                                                       const ECreatureUpdateAppearanceType appearanceType)
{
    Q_EMIT creatureAppearanceUpdated(creature, appearanceType);
}

void TCreatureStorage::publishCreatureUpdated(const std::shared_ptr<TCreature>& row,
                                            const ECreatureUpdateAppearanceType appearanceType)
{
    if (!row)
        return;
    notifyCreatureUpdated(row);
    if (appearanceType != ECreatureUpdateAppearanceType::None)
        notifyCreatureAppearanceUpdated(row, appearanceType);
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

} // namespace tibia::creatures
