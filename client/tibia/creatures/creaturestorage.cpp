#include "creatures/creaturestorage.h"

#include "appearances/appearanceinstance.h"
#include <tibia/config/runtimegameconfig.h>
#include "protocol/protocolcodes.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "tibia/creatures/attachedeffect.h"
#include "creatures/creaturespeedformula.h"
#include "game/localplayersessionthing.h"
#include "tibia/creatures/paperdoll.h"
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
    , m_movementTween(*this)
    , m_creatureId(id)
{
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
    if (!m_walking) {
        m_position = player.getPosition();
        setDirection(static_cast<uint8_t>(player.getDirection()));
        refreshAnimationPhases();
    }
    m_isLocalPlayer = true;
    m_softDead = tibia::game::localPlayerIsSoftDead();
    tibia::game::syncLocalPlayerStorageRow(*this);
}

bool TCreature::isWalkLocked() const
{
    return m_walkLockExpiration != 0 && g_clock.millis() < m_walkLockExpiration;
}

void TCreature::lockWalk(const uint16_t millis)
{
    m_walkLockExpiration = std::max<ticks_t>(m_walkLockExpiration, g_clock.millis() + millis);
}

const Position& TCreature::pushPreWalk(const Position& pos)
{
    m_preWalks.push_back(pos);
    return m_preWalks.back();
}

void TCreature::popPreWalkFront()
{
    if (!m_preWalks.empty())
        m_preWalks.pop_front();
}

void TCreature::clearAutoWalk()
{
    m_autoWalkDestination = {};
    m_lastAutoWalkPosition = {};
    m_autoWalkPathMode = tibia::client::AutoWalkPathMinimap;
    m_autoWalkRetries = 0;
    m_autoWalkDirs.clear();
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
    m_position = position;
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
}

void TCreature::setLight(const Light& light)
{
    m_light = light;
}

void TCreature::setSpeed(const uint16_t speed)
{
    if (m_speed == speed)
        return;
    m_speed = speed;
    m_calculatedStepSpeed = calculatedStepSpeed(speed);
    m_speedCalculator.invalidateCache();
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
    for (const PaperdollPtr& paperdoll : m_paperdolls) {
        if (paperdoll)
            paperdoll->setDirection(static_cast<tibia::client::Direction>(m_patternX));
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

void TCreature::setShader(const std::string& shader)
{
    m_shader = shader;
}

void TCreature::setTyping(const bool typing)
{
    m_typing = typing;
}

bool TCreature::hasPaperdoll(const uint16_t id) const
{
    return std::any_of(m_paperdolls.begin(), m_paperdolls.end(),
                       [id](const PaperdollPtr& pd) { return pd && pd->getId() == id; });
}

void TCreature::attachPaperdoll(const PaperdollPtr& paperdoll)
{
    if (!paperdoll)
        return;
    paperdoll->setDirection(static_cast<tibia::client::Direction>(m_patternX));
    uint_fast8_t i = 0;
    for (const auto& pd : m_paperdolls) {
        if (paperdoll->getPriority() < pd->getPriority())
            break;
        ++i;
    }
    m_paperdolls.insert(m_paperdolls.begin() + i, paperdoll);
}

bool TCreature::detachPaperdollById(const uint16_t id)
{
    const auto it = std::find_if(m_paperdolls.begin(), m_paperdolls.end(),
                                 [id](const PaperdollPtr& obj) { return obj && obj->getId() == id; });
    if (it == m_paperdolls.end())
        return false;
    m_paperdolls.erase(it);
    return true;
}

bool TCreature::detachPaperdollByPriority(const uint8_t priority)
{
    bool found = false;
    for (auto it = m_paperdolls.begin(); it != m_paperdolls.end();) {
        if (*it && (*it)->getPriority() == priority) {
            it = m_paperdolls.erase(it);
            found = true;
        } else {
            ++it;
        }
    }
    return found;
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

void TCreatureStorage::applyMoveCreature(const TCreatureID id, const Position& from, const Position& to)
{
    if (id == 0 || !to.isValid())
        return;

    const auto row = getOrCreate(id);
    if (!row)
        return;

    // Local predicted-step confirm: matching prewalk keeps the in-flight tween.
    if (row->isLocalPlayer()) {
        row->setAutoWalkRetries(0);
        if (row->isPreWalking() && !row->preWalks().empty() && to == row->preWalks().front()) {
            row->popPreWalkFront();
            return;
        }
        row->clearPreWalks();
    }

    if (from.isValid() && from.isInRange(to, 1, 1))
        row->beginWalk(from, to);
    else
        row->setPosition(to);
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

void protocolAttachPaperdoll(const TCreatureID id, const PaperdollPtr& paperdoll)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage || !paperdoll)
        return;
    const auto row = storage->getOrCreate(id);
    if (row->hasPaperdoll(paperdoll->getId()))
        return;
    row->attachPaperdoll(paperdoll);
    storage->publishCreatureUpdated(row);
}

void protocolDetachPaperdollById(const TCreatureID id, const uint16_t paperdollId)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->detachPaperdollById(paperdollId);
    storage->publishCreatureUpdated(row);
}

void protocolDetachPaperdollByPriority(const TCreatureID id, const uint8_t priority)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->detachPaperdollByPriority(priority);
    storage->publishCreatureUpdated(row);
}

void protocolSetShader(const TCreatureID id, const std::string& shader)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setShader(shader);
    storage->publishCreatureUpdated(row);
}

void protocolSetTyping(const TCreatureID id, const bool typing)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->getOrCreate(id);
    row->setTyping(typing);
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
