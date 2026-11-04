#include "creatures/creaturestorage.h"

#include "appearances/appearanceinstance.h"
#include "client/gameconfig.h"
#include "client/protocolcodes.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "tibia/world/attachedeffect.h"
#include "tibia/world/creature.h"
#include "tibia/world/localplayer.h"
#include "tibia/world/paperdoll.h"
#include "worldmap/worldmapfield.h"
#include <tibia/game/gamesession.h>
#include <tibia/world/map.h>

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

void TCreature::syncHudMetadataFrom(Creature& creature)
{
    // Phase 5.0-C: type / isNpc / vocation / icons / informationColor are protocol-
    // owned on storage. Keep-alive Map `Creature` may be a hollow generic instance.
    m_icon = creature.getIcon();
    m_inspectionType = creature.getInspectionType();
    m_paperdolls = creature.getPaperdolls();
    if (creature.getEchoRaidColor() != 0 || m_echoRaidColor == 0)
        m_echoRaidColor = creature.getEchoRaidColor();
}

void TCreature::syncAttachedFxFrom(Creature& creature)
{
    m_attachedEffects = creature.getAttachedEffects();
}

void TCreature::rebuildOutfitAppearance()
{
    ThingType* type = tibia::game::rawThingType(m_outfit.getId(), m_outfit.getCategory());
    m_outfitAppearance = appearances::TAppearanceInstanceFactory::fromOutfit(m_outfit, type);
    if (const auto outfitInstance =
            std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(m_outfitAppearance))
        outfitInstance->setOutfitLook(m_outfit, m_patternX);
}

void TCreature::syncKeepAliveExtrasFrom(Creature& creature)
{
    m_icon = creature.getIcon();
    m_inspectionType = creature.getInspectionType();
    m_paperdolls = creature.getPaperdolls();
    syncAttachedFxFrom(creature);
    m_showTimedSquare = creature.isTimedSquareVisible();
    m_timedSquareColor = creature.getTimedSquareColor();
    m_showStaticSquare = creature.isStaticSquareVisible();
    m_staticSquareColor = creature.getStaticSquareColor();
    m_hidden = creature.isHidden();
    m_hided = creature.isHided();
    m_canDraw = creature.canDraw();
    // Invisible players still need canBeSeen; prefer storage isPlayer over hollow Map.
    m_canBeSeen = !creature.isInvisible() || m_isPlayer;
    m_drawOutfitColor = creature.isDrawingOutfitColor();
    // patternX is storage-owned via setDirection — do not clobber from hollow Map
    // keep-alives that never received turn/walk (default Thing patternX = 0 / North).
    m_patternZ = creature.getPatternZ();
    m_displacement = creature.getDisplacement();
    m_jumpOffset = creature.getJumpOffset();
}

void TCreature::syncWorldDrawFrom(Creature& creature)
{
    // Walk tween (offset / walking / lastStep / walked pixels) is storage-owned — do not
    // overwrite from Map keep-alive while a step is in flight.
    if (!m_walking) {
        m_walkOffset = creature.getWalkOffset();
        m_lastStepFrom = creature.getLastStepFromPosition();
        m_lastStepTo = creature.getLastStepToPosition();
        m_walking = creature.isWalking();
        m_position = creature.getPosition();
        setDirection(static_cast<uint8_t>(creature.getDirection()));
        m_drawElevation = creature.getDrawElevation();
        m_animationPhase = creature.getCurrentAnimationPhase(false);
        m_mountAnimationPhase = creature.getCurrentAnimationPhase(true);
    }
    // Phase 5.0-C: passable / light / isPlayer / isNpc / dead stay protocol-owned.
    m_isLocalPlayer = creature.isLocalPlayer();
    m_softDead = m_isLocalPlayer && static_cast<LocalPlayer&>(creature).isSoftDead();
    syncKeepAliveExtrasFrom(creature);
    if (m_isLocalPlayer)
        syncLocalPlayerFrom(static_cast<LocalPlayer&>(creature));
}

void TCreature::syncLocalPlayerFrom(LocalPlayer& player)
{
    m_isLocalPlayer = true;
    m_mana = player.getMana();
    m_maxMana = player.getMaxMana();
    m_manaShield = player.getManaShield();
    m_maxManaShield = player.getMaxManaShield();
    m_harmony = player.getHarmony();
    m_serene = player.isSerene();
    m_vocation = player.getVocation();
    // Session character name is LocalPlayer-authoritative (CIP keeps it across
    // DeleteOnMap / KnownCreature refreshes that omit the name on the wire).
    if (const std::string& name = player.getName(); !name.empty())
        m_name = QString::fromStdString(name);
    // Walk mutation (preWalks / lock expiry / auto-walk) is storage-owned — do not
    // overwrite from LocalPlayer. Keep-alive still mirrors confirmed server tile.
    m_serverPosition = player.getServerPosition();
    // Walk timer / step duration: storage tween is authoritative while walking.
    if (!m_walking)
        m_stepDuration = player.getStepDuration();
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
    if (Creature::hasSpeedFormula()) {
        const double formulaSpeed = static_cast<double>(speed) * 2.0;
        if (formulaSpeed > -Creature::speedB) {
            m_calculatedStepSpeed = static_cast<uint16_t>(std::max<int>(
                1, static_cast<int>(std::floor((Creature::speedA * std::log((speed / 2.) + Creature::speedB)
                                                + Creature::speedC) + .5))));
        } else {
            m_calculatedStepSpeed = 1;
        }
    }
    m_stepCache.speed = 0; // invalidate duration cache
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
    }, g_gameConfig.getVolatileSquareDuration());
}

void TCreature::removeTimedSquare()
{
    m_showTimedSquare = false;
}

void TCreature::applyFrom(Creature& creature)
{
    m_creatureId = creature.getId();
    if (const std::string& name = creature.getName(); !name.empty())
        m_name = QString::fromStdString(name);
    if (creature.getEmblem() != 0)
        m_emblem = creature.getEmblem();
    // Protocol fields (health/outfit/speed/skull/shield/passable/light/type flags) are
    // storage-authoritative — do not clobber from hollow Map keep-alive.
    syncHudMetadataFrom(creature);
    syncWorldDrawFrom(creature);
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
        minZ = g_map.getFirstAwareFloor();
        maxZ = g_map.getLastAwareFloor();
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
    const LocalPlayerPtr player = tibia::game::localPlayer();
    if (player && player->getId() == id) {
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

void TCreatureStorage::syncWorldDrawFrom(Creature& creature)
{
    const TCreatureID id = creature.getId();
    if (id == 0)
        return;
    getOrCreate(id)->syncWorldDrawFrom(creature);
}

void TCreatureStorage::syncLocalPlayerStatsFromMap()
{
    const LocalPlayerPtr player = tibia::game::localPlayer();
    if (!player || player->getId() == 0)
        return;
    syncWorldDrawFrom(*player);
    if (const std::shared_ptr<TCreature> row = creatureAt(player->getId()))
        Q_EMIT creatureUpdated(std::weak_ptr<TCreature>(row));
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

void TCreatureStorage::upsertFromMapCreature(Creature& creature)
{
    const TCreatureID id = creature.getId();
    if (id == 0)
        return;

    const bool isNew = !m_creatures.contains(id);
    const Outfit previousOutfit = isNew ? Outfit{} : m_creatures.value(id)->outfit();

    const auto row = getOrCreate(id);
    row->applyFrom(creature);

    const auto weak = std::weak_ptr<TCreature>(row);
    Q_EMIT creatureUpdated(weak);

    if (creature.isPlayer() && isNew)
        Q_EMIT playerAdded(weak);

    if (!isNew && row->outfit() != previousOutfit)
        Q_EMIT creatureAppearanceUpdated(weak, ECreatureUpdateAppearanceType::FullOutfit);
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
        // KnownCreature row; if we just (re)created the LocalPlayer slot, recover
        // the session character name so the nameplate does not go blank.
        if (const LocalPlayerPtr player = tibia::game::localPlayer();
            player && player->getId() == id && !player->getName().empty()) {
            row->setName(QString::fromStdString(player->getName()));
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
    if (tibia::worldmap::TWorldMapField* field = g_map.getTile(row->position()))
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

void syncMapCreatureIfLive(Creature& creature)
{
    if (TCreatureStorage* storage = TCreatureStorage::instance())
        storage->upsertFromMapCreature(creature);
}

void syncKeepAliveExtrasIfLive(Creature& creature)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    const TCreatureID id = creature.getId();
    if (id == 0)
        return;
    const auto row = storage->getOrCreate(id);
    row->syncKeepAliveExtrasFrom(creature);
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
