// Host API v9: the session table (abi/session_api.h) over the game client's current
// CipSoft game session (game/currentgamesession.h). Split out of host_api_v1.cpp.
#include "mods/abi_internal.h"
#include "qtclient/abi/const.h"

#include "game/creaturesession.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/objectreference.h"
#include "game/playerdata.h"
#include "game/sessionhooks.h"
#include "container/containerciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "gamewindow/imbuingdialogcontroller.h"
#include "gamewindow/npctradecontroller.h"
#include "imbuing/imbuingstorage.h"
#include "input/playermovementintent.h"
#include "input/playermovementintenthandler.h"
#include "protocol/clientmessages.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "clockservice.h"
#include <client/outfit.h>
#include "game/mapsession.h"
#include "game/thingsession.h"
#include "minimap/waypointcosts.h"
#include "game/gamenotify.h"
#include "creatures/creaturestorage.h"
#include "teamfinder/teamfinderciptypes.h"

#include <client/const.h>
#include <client/position.h>
#include <teamfinder/teamfinder_types.h>
#include <tibia/assets/things.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <worldmap/worldmapfield.h>

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tibia::mods {
namespace {

tibia::game::MapPos toGameMapPos(const QtClientMapPos p)
{
    return {p.x, p.y, p.z};
}

Position toTipPosition(const QtClientMapPos p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

void fillAbiInventory(const tibia::container::TInventorySlot* src, QtClientInventorySlotInfo* dst)
{
    if (!dst)
        return;
    dst->empty = !src || src->empty() ? 1 : 0;
    dst->type_id = src ? src->typeId : 0;
    dst->stackable = src && src->stackable ? 1 : 0;
    dst->count = src ? src->count : 0;
    dst->charges = src ? src->charges : 0;
}

void fillAbiOutfit(const tibia::game::CreatureOutfitInfo& src, QtClientCreatureOutfitInfo* dst)
{
    if (!dst)
        return;
    dst->exists = src.exists ? 1 : 0;
    dst->is_object = src.isObject ? 1 : 0;
    dst->looktype = src.looktype;
    dst->head = src.head;
    dst->body = src.body;
    dst->legs = src.legs;
    dst->feet = src.feet;
    dst->addons = src.addons;
}

void fillAbiSnapshot(const tibia::game::CreatureSnapshot& src, QtClientCreatureSnapshot* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->id = src.id;
    const QByteArray nameUtf8 = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, nameUtf8.constData(), sizeof(dst->name) - 1);
    dst->pos = {src.pos.x, src.pos.y, src.pos.z};
    dst->health_percent = src.healthPercent;
    dst->shield = src.shield;
    dst->vocation = src.vocation;
    dst->creature_type = src.creatureType;
    dst->is_player = src.isPlayer ? 1 : 0;
    dst->is_npc = src.isNpc ? 1 : 0;
    dst->can_be_seen = src.canBeSeen ? 1 : 0;
    dst->is_dead = src.isDead ? 1 : 0;
    dst->is_local_player = src.isLocalPlayer ? 1 : 0;
    fillAbiOutfit(src.outfit, &dst->outfit);
}

void fillAbiRaceOutfit(const tibia::game::RaceOutfitInfo& src, QtClientRaceOutfitInfo* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->exists = src.exists ? 1 : 0;
    dst->is_object = src.isObject ? 1 : 0;
    dst->looktype = src.looktype;
    dst->head = src.head;
    dst->body = src.body;
    dst->legs = src.legs;
    dst->feet = src.feet;
    dst->addons = src.addons;
    const QByteArray n = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, n.constData(), sizeof(dst->name) - 1);
}

void fillAbiQuest(const tibia::game::QuestInfo& src, QtClientQuestInfo* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->exists = src.exists ? 1 : 0;
    dst->id = src.id;
    const QByteArray n = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, n.constData(), sizeof(dst->name) - 1);
}

TeamFinderAssemble teamFinderFromAbi(const QtClientTeamFinderAssemble* a)
{
    if (!a)
        return {};
    TeamFinderAssemble t{};
    t.minLevel = a->min_level;
    t.maxLevel = a->max_level;
    t.vocationIDs = a->vocation_ids;
    t.teamSlots = a->team_slots;
    t.freeSlots = a->free_slots;
    t.partyBool = a->party_bool != 0;
    t.timestamp = a->timestamp;
    t.teamType = a->team_type;
    t.bossID = a->boss_id;
    t.huntType = a->hunt_type;
    t.huntArea = a->hunt_area;
    t.questID = a->quest_id;
    return t;
}

thread_local std::optional<tibia::game::TObjectReference> g_sessFindPlayerItemLast;

// --- Host peels: the session ABI over the current CipSoft session ---
//
// Each login, replay or tutorial is a new session (game/currentgamesession.h):
// these read the current one at call time and do nothing between sessions.

namespace gameclient = tibia::protocol::gameclient;

tibia::game::TGameSessionBase* liveSession() { return tibia::game::currentGameSession(); }

/// Online, with a living character and a connected game connection.
bool canPerformGameAction()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session && session->canPerformGameAction();
}

bool hasLocalPlayer()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session && session->hasLocalPlayer();
}

/// The own creature (null before the server described it).
std::shared_ptr<tibia::creatures::TCreature> localPlayerCreature()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    if (!session || !session->hasLocalPlayer() || !session->creatureStorage())
        return nullptr;
    return session->creatureStorage()->player();
}

tibia::input::TPlayerMovementIntentHandler* movementIntent()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session ? session->movementIntent() : nullptr;
}

/// The inventory slot (null without a local player).
const tibia::container::TInventorySlot* inventorySlot(const uint8_t slot)
{
    tibia::container::TInventoryContainer* inventory = tibia::game::currentInventoryContainer();
    if (!hasLocalPlayer() || !inventory)
        return nullptr;
    return &inventory->slotAt(static_cast<int>(slot));
}

int emptyContainerId()
{
    tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    return storage ? storage->findEmptyId() : 0;
}

/// Replay / screenshot harness only: `QTCLIENT_DEBUG_WORLD_LIGHT=<intensity>[:<color>]`
/// pins the world ambient light intensity (session dumps keep whatever time
/// they were recorded at).
std::optional<uint8_t> debugWorldLightIntensity()
{
    static const std::optional<uint8_t> pinned = []() -> std::optional<uint8_t> {
        const QByteArray spec = qgetenv("QTCLIENT_DEBUG_WORLD_LIGHT");
        if (spec.isEmpty())
            return std::nullopt;
        bool ok = false;
        const int intensity = spec.split(':').value(0).trimmed().toInt(&ok);
        if (!ok)
            return std::nullopt;
        return static_cast<uint8_t>(std::clamp(intensity, 0, 255));
    }();
    return pinned;
}

/// The first inventory or open-container object of `itemId` (any subtype when -1).
std::optional<tibia::game::TObjectReference> findPlayerItem(const uint32_t itemId, const int subType)
{
    using tibia::container::TInventorySlot;
    if (tibia::container::TInventoryContainer* inventory = tibia::game::currentInventoryContainer()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
            const TInventorySlot& row = inventory->slotAt(slot);
            if (row.empty() || row.typeId != itemId)
                continue;
            const uint8_t fluidOrSub = row.fluidOrSplash ? row.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            return tibia::game::TObjectReference{ row.typeId, Position(UINT16_MAX, static_cast<uint16_t>(slot), 0), 0,
                                                  static_cast<uint16_t>(row.stackable ? row.count : 0), row.tier, fluidOrSub };
        }
    }
    tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    if (!storage)
        return std::nullopt;
    for (const tibia::container::TContainerID containerId : storage->openIds()) {
        const tibia::container::TStoredContainer* row = storage->containerAt(containerId);
        if (!row)
            continue;
        for (size_t i = 0; i < row->items().size(); ++i) {
            const TInventorySlot& slot = row->items()[i];
            if (slot.empty() || slot.typeId != itemId || slot.tier != 0)
                continue;
            const uint8_t fluidOrSub = slot.fluidOrSplash ? slot.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            const int pageSlot = static_cast<int>(i);
            return tibia::game::TObjectReference{ slot.typeId,
                                                  Position(0xffff, static_cast<uint16_t>(containerId | 0x40), static_cast<uint8_t>(pageSlot)),
                                                  pageSlot, static_cast<uint16_t>(slot.stackable ? slot.count : 0), slot.tier, fluidOrSub };
        }
    }
    return std::nullopt;
}

/// The server's count of everything the player carries, else the inventory
/// and the open containers.
uint32_t localPlayerInventoryCount(const uint16_t itemId, const uint8_t tier)
{
    const tibia::game::TGameSessionBase* session = liveSession();
    if (!session || !session->hasLocalPlayer())
        return 0;
    if (const auto& counts = session->objectCountStorage(); counts && !counts->counts().empty()) {
        const auto it = counts->counts().find({ itemId, tier });
        if (it != counts->counts().end())
            return it->second;
    }
    uint64_t total = 0;
    const auto add = [&](const tibia::container::TInventorySlot& slot) {
        if (!slot.empty() && slot.typeId == itemId && slot.tier == tier)
            total += slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
    };
    if (tibia::container::TInventoryContainer* inventory = tibia::game::currentInventoryContainer()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot)
            add(inventory->slotAt(slot));
    }
    if (tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage()) {
        for (const tibia::container::TContainerID containerId : storage->openIds()) {
            if (const tibia::container::TStoredContainer* row = storage->containerAt(containerId)) {
                for (const tibia::container::TInventorySlot& slot : row->items())
                    add(slot);
            }
        }
    }
    return static_cast<uint32_t>(std::min<uint64_t>(total, std::numeric_limits<uint32_t>::max()));
}

void useMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    gameclient::sendUseItem(position, itemId, static_cast<uint8_t>(stackPos), emptyContainerId());
    tibia::game::notify("onUse", position, itemId, stackPos, 0);
}

// --- Session nested API ---

int sessIsOnline()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session && session->isOnline() ? 1 : 0;
}
int sessIsDead()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session && session->isDead() ? 1 : 0;
}
int sessPlayerPresent() { return hasLocalPlayer() ? 1 : 0; }

QtClientMapPos sessPlayerMapPos()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    const Position p = session ? session->localPlayerPosition() : Position();
    return QtClientMapPos{p.x, p.y, p.z};
}

int sessPlayerName(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    // The own creature carries the server's name (a replay logs in as "Player").
    std::string name;
    const tibia::game::TGameSessionBase* session = liveSession();
    const std::shared_ptr<tibia::creatures::TCreature> player =
        session && session->creatureStorage() ? session->creatureStorage()->player() : nullptr;
    if (player && !player->name().isEmpty())
        name = player->name().toStdString();
    else if (const tibia::game::TPlayerData* data = tibia::game::currentPlayerData(); data && !data->characterName().empty())
        name = data->characterName();
    else if (hasLocalPlayer())
        name = session->characterName().toStdString();
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

uint32_t sessPlayerId()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    if (!session || !session->hasLocalPlayer() || !session->creatureStorage())
        return 0;
    return session->creatureStorage()->playerID();
}

int sessPlayerLevel()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<int>(data->level()) : 0;
}

void sessStopWalk()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->stopWalking();
}

int sessIsWalking()
{
    const tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    return intent && intent->hasGoal() ? 1 : 0;
}

/// Walks to `goal` (map click, travel); the path may end on a goal that cannot
/// be stood on. `retry` and `path_mode` are kept for the ABI.
int sessAutoWalk(QtClientMapPos goal, int retry, int path_mode)
{
    (void)retry;
    (void)path_mode;
    const Position target = toTipPosition(goal);
    const tibia::game::TGameSessionBase* session = liveSession();
    tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    if (!intent || !target.isValid() || !session || !session->isOnline())
        return 0;
    intent->handleMovementIntent(std::make_shared<tibia::input::TMovementIntentTargetCoordinate>(target.x, target.y, target.z, true));
    return intent->hasGoal() ? 1 : 0;
}

void sessTalk(const char* text)
{
    // A say (CIP chat message mode Say).
    if (text && *text && canPerformGameAction())
        gameclient::sendTalk(1, 0, "", text);
}

void sessTurn(int direction)
{
    if (!canPerformGameAction())
        return;
    switch (static_cast<tibia::client::Direction>(direction)) {
    case tibia::client::North: gameclient::sendTurnNorth(); break;
    case tibia::client::East: gameclient::sendTurnEast(); break;
    case tibia::client::South: gameclient::sendTurnSouth(); break;
    case tibia::client::West: gameclient::sendTurnWest(); break;
    default: break;
    }
}

uint64_t sessMillis() { return static_cast<uint64_t>(tibia::shared::TClockService::frameMillis()); }
uint8_t sessSeaFloor() { return tibia::worldmap::TWorldMapCentralPosition::GroundFloor; }
uint8_t sessWorldLight()
{
    if (const std::optional<uint8_t> pinned = debugWorldLightIntensity())
        return *pinned;
    const tibia::worldmap::TWorldMapStorage* map = tibia::game::currentWorldMapStorage();
    return map ? map->worldLight().intensity : 0;
}
uint32_t sessItemRev()
{
    const tibia::worldmap::TWorldMapStorage* map = tibia::game::currentWorldMapStorage();
    return map ? map->itemRevision() : 0;
}

int sessHasReachedMain()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data && data->hasReachedMain() ? 1 : 0;
}

uint16_t sessInvTypeId(uint8_t slot)
{
    const tibia::container::TInventorySlot* row = inventorySlot(slot);
    return row ? row->typeId : 0;
}

void sessInvSlot(uint8_t slot, QtClientInventorySlotInfo* out)
{
    fillAbiInventory(inventorySlot(slot), out);
}

/// Uses the inventory slot's object (opens the container there).
void sessOpenInv(uint8_t slot)
{
    const tibia::container::TInventorySlot* row = inventorySlot(slot);
    if (!row || row->empty() || row->typeId == 0 || !canPerformGameAction())
        return;
    // The inventory slot's wire position: Position(UINT16_MAX, slot, 0).
    const Position position(UINT16_MAX, static_cast<uint16_t>(slot), 0);
    const uint16_t typeId = row->typeId;
    gameclient::sendUseItem(position, typeId, 0, emptyContainerId());
    tibia::game::notify("onUse", position, typeId, 0, 0);
}

uint8_t sessBlessVisual()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? data->blessVisualState() : 0;
}

void sessSetCreatureHidden(const uint32_t creatureId, const int hidden)
{
    tibia::creatures::TCreatureStorage* storage = tibia::game::currentCreatureStorage();
    if (!storage)
        return;
    const auto creature = storage->creatureAt(creatureId);
    if (!creature || creature->isDrawSuppressed() == (hidden != 0))
        return;
    creature->setDrawSuppressed(hidden != 0);
    storage->publishCreatureUpdated(creature);
}

uint32_t sessLpHealth()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint32_t>(data->health()) : 0;
}
uint32_t sessLpMaxHealth()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint32_t>(data->maxHealth()) : 0;
}
uint32_t sessLpMana()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint32_t>(data->mana()) : 0;
}
uint32_t sessLpMaxMana()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint32_t>(data->maxMana()) : 0;
}
uint16_t sessLpLevelPct()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint16_t>(data->levelPercent()) : 0;
}
uint16_t sessLpStamina()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint16_t>(data->stamina()) : 0;
}
uint8_t sessLpVocation()
{
    // The classic vocation id: promoted vocations count from 11.
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint8_t>(data->vocation() + (data->promoted() ? 10 : 0)) : 0;
}
uint8_t sessLpShield()
{
    const auto player = localPlayerCreature();
    return player ? player->shield() : 0;
}
uint64_t sessLpMoney()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint64_t>(data->totalMoney()) : 0;
}
uint32_t sessLpFreeCap()
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return data ? static_cast<uint32_t>(data->freeCapacity()) : 0;
}
uint32_t sessLpInvCount(uint16_t itemId) { return localPlayerInventoryCount(itemId, 0); }

void sessLpOutfit(QtClientCreatureOutfitInfo* out)
{
    if (!out)
        return;
    const auto player = localPlayerCreature();
    const Outfit o = player ? player->outfit() : Outfit();
    const bool isObject = o.getId() == 0 && o.getAuxId() != 0;
    const uint16_t looktype = isObject ? o.getAuxId() : o.getId();
    tibia::game::CreatureOutfitInfo info{};
    if (looktype != 0) {
        info.exists = true;
        info.isObject = isObject;
        info.looktype = looktype;
        info.head = o.getHead();
        info.body = o.getBody();
        info.legs = o.getLegs();
        info.feet = o.getFeet();
        info.addons = o.getAddons();
    }
    fillAbiOutfit(info, out);
}

int sessItemName(uint16_t id, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::itemName(id);
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessItemIsContainer(uint16_t id) { return tibia::game::itemIsContainer(id) ? 1 : 0; }
int sessItemIsPickupable(uint16_t id) { return tibia::game::itemIsPickupable(id) ? 1 : 0; }
int64_t sessItemMeanPrice(uint16_t id) { return tibia::game::itemMeanPrice(id); }
int64_t sessItemMaxNpcSale(uint16_t id) { return tibia::game::itemMaxNpcSaleGoldPrice(id); }

int sessRaceName(uint32_t raceId, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::raceName(raceId);
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessRaceExists(uint32_t raceId) { return tibia::game::raceExists(raceId) ? 1 : 0; }
int sessRaceIsBoss(uint32_t raceId) { return tibia::game::raceIsBoss(raceId) ? 1 : 0; }

void sessRaceOutfitAbi(uint32_t raceId, QtClientRaceOutfitInfo* out)
{
    if (!out)
        return;
    fillAbiRaceOutfit(tibia::game::raceOutfit(raceId), out);
}

int sessRaceIdsByName(const char* name, uint32_t* out_ids, int max_ids)
{
    if (!name || !out_ids || max_ids <= 0)
        return 0;
    const std::vector<uint32_t> ids = tibia::game::raceIdsByName(std::string(name));
    const int n = static_cast<int>(std::min(ids.size(), static_cast<size_t>(max_ids)));
    for (int i = 0; i < n; ++i)
        out_ids[i] = ids[static_cast<size_t>(i)];
    return n;
}

int sessAllRaceIds(uint32_t* out_ids, int max_ids)
{
    if (!out_ids || max_ids <= 0)
        return 0;
    const std::vector<uint32_t> ids = tibia::game::allRaceIds();
    const int n = static_cast<int>(std::min(ids.size(), static_cast<size_t>(max_ids)));
    for (int i = 0; i < n; ++i)
        out_ids[i] = ids[static_cast<size_t>(i)];
    return n;
}

void sessQuestInfoAbi(uint16_t id, QtClientQuestInfo* out)
{
    if (!out)
        return;
    fillAbiQuest(tibia::game::questInfo(id), out);
}

int sessAllQuestInfos(QtClientQuestInfo* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::QuestInfo> all = tibia::game::allQuestInfos();
    const int n = static_cast<int>(std::min(all.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i)
        fillAbiQuest(all[static_cast<size_t>(i)], &out[i]);
    return n;
}

void sessCreatureAt(QtClientMapPos pos, QtClientCreatureSnapshot* out)
{
    if (!out)
        return;
    std::memset(out, 0, sizeof(*out));
    tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toTipPosition(pos));
    if (!tile)
        return;
    uint32_t topCreature = 0;
    for (int i = tile->getThingCount() - 1; i >= 0; --i) {
        if (const uint32_t cid = tile->stackCreatureId(i)) {
            topCreature = cid;
            break;
        }
    }
    fillAbiSnapshot(tibia::game::creatureAt(topCreature), out);
}

void sessCreatureAtId(uint32_t creature_id, QtClientCreatureSnapshot* out)
{
    if (!out)
        return;
    fillAbiSnapshot(tibia::game::creatureAt(creature_id), out);
}

int sessSpectatorsNear(QtClientMapPos center, int multi_floor, int left, int right, int top, int bottom,
                       QtClientCreatureSnapshot* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::CreatureSnapshot> snaps =
        tibia::game::spectatorsNear(toGameMapPos(center), multi_floor != 0, left, right, top, bottom);
    const int n = static_cast<int>(std::min(snaps.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i)
        fillAbiSnapshot(snaps[static_cast<size_t>(i)], &out[i]);
    return n;
}

void sessAwareRange(QtClientMapAwareRange* out)
{
    if (!out)
        return;
    const auto r = tibia::game::awareRange();
    out->left = r.left;
    out->top = r.top;
    out->right = r.right;
    out->bottom = r.bottom;
}

int sessItemHits(QtClientMapPos pos, QtClientMapUseHit* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::MapUseHit> hits = tibia::game::itemHits(toGameMapPos(pos));
    const int n = static_cast<int>(std::min(hits.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i) {
        const auto& h = hits[static_cast<size_t>(i)];
        out[i].ok = h.ok ? 1 : 0;
        out[i].item_id = h.itemId;
        out[i].stack_pos = h.stackPos;
    }
    return n;
}

void sessTopUseHit(QtClientMapPos pos, QtClientMapUseHit* out)
{
    if (!out)
        return;
    const auto h = tibia::game::topUseHit(toGameMapPos(pos));
    out->ok = h.ok ? 1 : 0;
    out->item_id = h.itemId;
    out->stack_pos = h.stackPos;
}

int sessFindItemsInRange(QtClientMapPos center, int range_x, int range_y, uint16_t type_id_or_0,
                         QtClientMapItemHit* out, int max_out)
{
    if (!out || max_out <= 0 || range_x < 0 || range_y < 0)
        return 0;
    const Position c = toTipPosition(center);
    if (!c.isValid())
        return 0;

    int written = 0;
    for (int x = c.x - range_x; x <= c.x + range_x && written < max_out; ++x) {
        for (int y = c.y - range_y; y <= c.y + range_y && written < max_out; ++y) {
            tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(x, y, c.z));
            if (!tile)
                continue;
            const int count = tile->getThingCount();
            for (int sp = 0; sp < count && written < max_out; ++sp) {
                if (tile->stackCreatureId(sp) != 0)
                    continue;
                const uint16_t clientId = tile->itemClientIdAt(sp);
                if (clientId == 0)
                    continue;
                if (type_id_or_0 != 0 && clientId != type_id_or_0)
                    continue;
                if (!tibia::appearances::TAppearancesManager::instance().isValidDatId(clientId, ThingCategoryItem))
                    continue;
                out[written].pos = {x, y, static_cast<int>(c.z)};
                out[written].type_id = clientId;
                out[written].stack_pos = sp;
                ++written;
            }
        }
    }
    return written;
}

int sessFindItemsInRangeTypes(QtClientMapPos center, int range_x, int range_y, const uint16_t* type_ids,
                              int type_count, QtClientMapItemHit* out, int max_out)
{
    if (!out || max_out <= 0 || range_x < 0 || range_y < 0)
        return 0;
    const Position c = toTipPosition(center);
    if (!c.isValid())
        return 0;

    int written = 0;
    for (int x = c.x - range_x; x <= c.x + range_x && written < max_out; ++x) {
        for (int y = c.y - range_y; y <= c.y + range_y && written < max_out; ++y) {
            tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(x, y, c.z));
            if (!tile)
                continue;
            const int count = tile->getThingCount();
            for (int sp = 0; sp < count && written < max_out; ++sp) {
                if (tile->stackCreatureId(sp) != 0)
                    continue;
                const uint16_t clientId = tile->itemClientIdAt(sp);
                if (clientId == 0)
                    continue;
                if (type_count > 0) {
                    bool match = false;
                    for (int t = 0; t < type_count; ++t) {
                        if (clientId == type_ids[static_cast<size_t>(t)]) {
                            match = true;
                            break;
                        }
                    }
                    if (!match)
                        continue;
                }
                if (!tibia::appearances::TAppearancesManager::instance().isValidDatId(clientId, ThingCategoryItem))
                    continue;
                out[written].pos = {x, y, static_cast<int>(c.z)};
                out[written].type_id = clientId;
                out[written].stack_pos = sp;
                ++written;
            }
        }
    }
    return written;
}

int sessSightClear(QtClientMapPos from, QtClientMapPos to)
{
    return tibia::game::isSightClear(toGameMapPos(from), toGameMapPos(to)) ? 1 : 0;
}

int sessWalkable(QtClientMapPos pos, int ignore_creatures)
{
    return tibia::game::isWalkable(toGameMapPos(pos), ignore_creatures != 0) ? 1 : 0;
}

size_t sessStoredFieldsOnFloor(int z, QtClientMapFieldInfo* out, size_t cap)
{
    const std::vector<tibia::game::MapFieldInfo> fields = tibia::game::storedFieldsOnFloor(z);
    for (size_t i = 0; out && i < fields.size() && i < cap; ++i) {
        const tibia::game::MapFieldInfo& f = fields[i];
        out[i] = QtClientMapFieldInfo{ { f.pos.x, f.pos.y, f.pos.z }, f.groundSpeed,
                                       static_cast<uint8_t>(f.walkable ? 1 : 0), static_cast<uint8_t>(f.pathable ? 1 : 0) };
    }
    return fields.size();
}

int sessIsAwareOfPosition(QtClientMapPos pos)
{
    return tibia::game::isAwareOfPosition(toGameMapPos(pos)) ? 1 : 0;
}

using WaypointCostsRef = std::shared_ptr<tibia::minimap::TWaypointCosts>;

QtClientHandle sessWaypointCostsSnapshot()
{
    WaypointCostsRef costs = tibia::game::waypointCostsSnapshot();
    return QtClientHandle{ costs ? new WaypointCostsRef(std::move(costs)) : nullptr };
}

int sessWaypointCostAt(QtClientHandle costs, int x, int y, int z)
{
    auto* ref = static_cast<WaypointCostsRef*>(costs.ptr);
    return ref && *ref ? (*ref)->costAt(x, y, z) : tibia::minimap::TWaypointCosts::UnknownCost;
}

void sessWaypointCostsRelease(QtClientHandle costs)
{
    delete static_cast<WaypointCostsRef*>(costs.ptr);
}

uint16_t sessStaticMapSubAreaAt(QtClientMapPos pos)
{
    return tibia::game::staticMapSubAreaAt(toGameMapPos(pos));
}

int sessStaticMapArea(uint16_t id, int subarea, QtClientStaticMapArea* out)
{
    if (!out)
        return 0;
    tibia::game::StaticMapArea area;
    if (!tibia::game::staticMapArea(id, subarea != 0, area))
        return 0;
    *out = QtClientStaticMapArea{};
    out->id = area.id;
    out->area_id = area.areaId;
    out->is_subarea = area.isSubArea ? 1 : 0;
    std::snprintf(out->name, sizeof out->name, "%s", area.name.c_str());
    std::snprintf(out->alias, sizeof out->alias, "%s", area.alias.c_str());
    out->label = QtClientMapPos{ area.label.x, area.label.y, area.label.z };
    out->has_mask_file = area.hasMaskFile ? 1 : 0;
    out->mask_origin = QtClientMapPos{ area.maskOrigin.x, area.maskOrigin.y, area.maskOrigin.z };
    out->mask_width = area.maskWidth;
    out->mask_height = area.maskHeight;
    return 1;
}

size_t copyIds(const std::vector<uint16_t>& ids, uint16_t* out, size_t cap)
{
    for (size_t i = 0; out && i < ids.size() && i < cap; ++i)
        out[i] = ids[i];
    return ids.size();
}

size_t sessStaticMapAreaIds(int subareas, uint16_t* out, size_t cap)
{
    return copyIds(tibia::game::staticMapAreaIds(subareas != 0), out, cap);
}

size_t sessStaticMapSubAreasOfArea(uint16_t area_id, uint16_t* out, size_t cap)
{
    return copyIds(tibia::game::staticMapSubAreasOfArea(area_id), out, cap);
}

void sessSafeLogout()
{
    if (const tibia::game::TGameSessionBase* session = liveSession(); session && session->isOnline())
        gameclient::sendLogout();
}
void sessRequestBless()
{
    if (canPerformGameAction())
        gameclient::sendRequestBless();
}
void sessPartyJoin(uint32_t id)
{
    if (canPerformGameAction())
        gameclient::sendJoinParty(id);
}
void sessSetAttacking(uint32_t id)
{
    if (tibia::game::TGameSessionBase* session = liveSession())
        session->setAttackingCreatureId(id);
}
uint32_t sessAttackingId()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session ? session->attackingCreatureId() : 0;
}

int sessCharacterName(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const tibia::game::TGameSessionBase* session = liveSession();
    const std::string name = session ? session->characterName().toStdString() : std::string();
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessIsSwitchingChar()
{
    const tibia::game::TGameSessionBase* session = liveSession();
    return session && session->isSwitchingCharacter() ? 1 : 0;
}

void sessQuickLootBw(uint8_t filter, uint16_t size, const uint16_t* listed_items, int listed_count)
{
    if (!listed_items || listed_count < 0)
        return;
    std::vector<uint16_t> v(static_cast<size_t>(listed_count));
    for (int i = 0; i < listed_count; ++i)
        v[static_cast<size_t>(i)] = listed_items[static_cast<size_t>(i)];
    (void)size;
    if (!canPerformGameAction())
        return;
    // The vector's length is the count: a size that disagrees with the list
    // desyncs the protocol (the server reads the wrong byte count).
    gameclient::requestQuickLootBlackWhiteList(filter, static_cast<uint16_t>(v.size()), v);
}

void sessOpenQuickLoot(uint8_t action, uint8_t category, QtClientMapPos pos, uint16_t item_id, uint8_t stackpos,
                       int use_main_as_fallback)
{
    if (canPerformGameAction())
        gameclient::openContainerQuickLoot(action, category, toTipPosition(pos), item_id, stackpos, use_main_as_fallback != 0);
}

void sessStatusBestiary(uint16_t race_id, int status)
{
    if (canPerformGameAction())
        gameclient::sendStatusTrackerBestiary(race_id, status != 0);
}

void sessBestiarySearch(uint16_t race_id)
{
    if (!canPerformGameAction())
        return;
    if (const tibia::game::TGameSessionBase* session = liveSession(); session && session->cyclopediaProtocolMessageHandler())
        session->cyclopediaProtocolMessageHandler()->requestMonsterCyclopediaRace(race_id);
}

void sessOpenPrivate(const char* receiver_utf8)
{
    if (!receiver_utf8 || !*receiver_utf8 || !canPerformGameAction())
        return;
    gameclient::sendOpenPrivateChannel(receiver_utf8);
}

// Team finder (C 0x2C member / 0x2D leader actions).
void sessTfList()
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderMemberAction(0);
}
void sessTfLeader()
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderLeaderAction(0);
}
void sessTfJoin(uint32_t id)
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderMemberAction(1, id);
}
void sessTfLeave(uint32_t id)
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderMemberAction(2, id);
}

void sessTfCreate(const QtClientTeamFinderAssemble* a)
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderLeaderAction(3, 0, 0, teamFinderFromAbi(a));
}

void sessTfResetLeader()
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderLeaderAction(1);
}

void sessTfUpdateMember(uint32_t member_id, uint8_t status)
{
    if (canPerformGameAction())
        gameclient::sendTeamFinderLeaderAction(2, member_id, status);
}

void sessUseInvOnCreature(uint16_t item_id, uint32_t creature_id)
{
    if (!canPerformGameAction() || !creature_id)
        return;
    const Position pos(0xFFFF, 0, 0);
    gameclient::sendUseOnCreature(pos, item_id, 0, creature_id);
    tibia::game::notify("onUseWith", pos, item_id, creature_id, 0);
}

void sessUseMapObject(QtClientMapPos pos, uint16_t item_id, int stack_pos)
{
    useMapObject(toTipPosition(pos), item_id, stack_pos);
}

void sessUseInvWithMap(uint16_t item_id, QtClientMapPos to_pos, uint16_t to_item_id, int to_stack_pos)
{
    const Position to = toTipPosition(to_pos);
    if (!canPerformGameAction() || !to.isValid() || to_item_id == 0 || to_stack_pos < 0)
        return;
    const Position pos(0xFFFF, 0, 0);
    gameclient::sendUseItemWith(pos, item_id, 0, to, to_item_id, static_cast<uint8_t>(to_stack_pos));
    tibia::game::notify("onUseWith", pos, item_id, to, 0);
}

int sessLpDirection()
{
    if (!hasLocalPlayer())
        return QTCLIENT_DIRECTION_INVALID;
    const auto player = localPlayerCreature();
    return player ? static_cast<int>(player->patternX()) : static_cast<int>(tibia::client::InvalidDirection);
}

void sessHoldDirection(const int direction)
{
    if (direction < QTCLIENT_DIRECTION_NORTH || direction >= QTCLIENT_DIRECTION_INVALID)
        return;
    const tibia::game::TGameSessionBase* session = liveSession();
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent(); intent && session && session->isOnline())
        intent->handleMovementIntent(std::make_shared<tibia::input::TMovementIntentDirection>(direction));
}

void sessReleaseDirection()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->handleMovementIntent(std::make_shared<tibia::input::TMovementIntentStop>());
}

int sessCreatureTypeIsSummon(uint8_t t) { return tibia::game::creatureTypeIsSummon(t) ? 1 : 0; }

QtClientHandle sessFindPlayerItem(uint32_t item_id, int sub_type)
{
    g_sessFindPlayerItemLast = findPlayerItem(item_id, sub_type);
    QtClientHandle h{};
    h.ptr = g_sessFindPlayerItemLast ? &*g_sessFindPlayerItemLast : nullptr;
    return h;
}


int sessCharacterConfigurationLoaded() { return tibia::game::currentCharacterConfigurationLoaded() ? 1 : 0; }

// --- Team Finder (v7): the current session's storage as plain rows ---

void copyName(char* out, size_t len, const QString& name)
{
    const QByteArray utf8 = name.toUtf8();
    std::snprintf(out, len, "%s", utf8.constData());
}

void tfParamsToAbi(const tibia::teamfinder::TTeamParameters& p, QtClientTeamFinderParams* out)
{
    *out = QtClientTeamFinderParams{};
    out->min_level = p.minLevel;
    out->max_level = p.maxLevel;
    for (const uint8_t vocation : p.vocations) {
        if (vocation < 32)
            out->vocation_mask |= 1u << vocation;
    }
    out->team_slots = p.teamSlots;
    out->free_slots = p.freeSlots;
    out->start_time = p.startTime;
    out->activity_kind = static_cast<uint8_t>(p.activity.kind);
    out->boss_id = p.activity.bossId;
    out->hunt_type = p.activity.huntType;
    out->hunt_area = p.activity.huntArea;
    out->quest_id = p.activity.questId;
    out->include_party = p.includeParty ? 1 : 0;
}

int sessTfParams(QtClientTeamFinderParams* out)
{
    tibia::teamfinder::TTeamFinderStorage* storage = tibia::game::currentTeamFinderStorage();
    if (!storage || !out)
        return 0;
    const tibia::teamfinder::TTeamParameters params = storage->teamParameters();
    tfParamsToAbi(params, out);
    return params.teamSlots != 0xffff ? 1 : 0;
}

size_t sessTfMembers(QtClientTeamFinderMember* out, size_t cap)
{
    tibia::teamfinder::TTeamFinderStorage* storage = tibia::game::currentTeamFinderStorage();
    if (!storage)
        return 0;
    const auto members = storage->members();
    for (size_t i = 0; out && i < members.size() && i < cap; ++i) {
        const auto& m = members[i];
        out[i] = QtClientTeamFinderMember{};
        out[i].character_id = m.characterId;
        copyName(out[i].name, sizeof(out[i].name), m.name);
        out[i].level = m.level;
        out[i].vocation = m.vocation;
        out[i].status = m.status;
    }
    return members.size();
}

size_t sessTfTeams(QtClientTeamFinderTeam* out, size_t cap)
{
    tibia::teamfinder::TTeamFinderStorage* storage = tibia::game::currentTeamFinderStorage();
    if (!storage)
        return 0;
    const auto teams = storage->teams();
    for (size_t i = 0; out && i < teams.size() && i < cap; ++i) {
        const auto& t = teams[i];
        out[i] = QtClientTeamFinderTeam{};
        out[i].team_id = t.teamId;
        copyName(out[i].leader_name, sizeof(out[i].leader_name), t.leaderName);
        tfParamsToAbi(t.parameters, &out[i].params);
        out[i].status = t.status;
    }
    return teams.size();
}

// --- v9: containers, quick loot, imbuing, NPC trade options, attack mark ---

size_t sessOpenContainerIds(uint8_t* out, size_t cap)
{
    tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    if (!storage)
        return 0;
    QList<tibia::container::TContainerID> ids = storage->openIds();
    std::sort(ids.begin(), ids.end());
    for (size_t i = 0; out && i < static_cast<size_t>(ids.size()) && i < cap; ++i)
        out[i] = static_cast<uint8_t>(ids[static_cast<qsizetype>(i)]);
    return static_cast<size_t>(ids.size());
}

const tibia::container::TStoredContainer* openContainer(const uint8_t id)
{
    tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    return storage ? storage->containerAt(static_cast<tibia::container::TContainerID>(id)) : nullptr;
}

int sessContainerInfo(uint8_t container_id, QtClientContainerInfo* out)
{
    const tibia::container::TStoredContainer* row = openContainer(container_id);
    if (!row || !out)
        return 0;
    *out = QtClientContainerInfo{};
    out->container_id = container_id;
    out->type_id = row->containerItem().typeId;
    out->capacity = row->capacity();
    out->size = row->size();
    out->first_index = row->firstIndex();
    out->has_parent = row->hasParent() ? 1 : 0;
    out->has_pages = row->hasPages() ? 1 : 0;
    copyUtf8(out->name, sizeof out->name, row->name());
    return 1;
}

size_t sessContainerItems(uint8_t container_id, QtClientInventorySlotInfo* out, size_t cap)
{
    const tibia::container::TStoredContainer* row = openContainer(container_id);
    if (!row)
        return 0;
    const std::vector<tibia::container::TInventorySlot>& items = row->items();
    for (size_t i = 0; out && i < items.size() && i < cap; ++i)
        fillAbiInventory(&items[i], &out[i]);
    return items.size();
}

int sessQuickLootListType()
{
    const tibia::quickloot::TQuickLootBlackWhitelistStorage* storage = tibia::game::currentQuickLootBlackWhitelistStorage();
    return storage ? storage->listType() : QTCLIENT_QUICK_LOOT_BLACKLIST;
}

size_t sessQuickLootList(int list_type, uint16_t* out, size_t cap)
{
    const tibia::quickloot::TQuickLootBlackWhitelistStorage* storage = tibia::game::currentQuickLootBlackWhitelistStorage();
    if (!storage)
        return 0;
    const std::set<uint16_t>& list = storage->list(list_type);
    size_t i = 0;
    for (const uint16_t id : list) {
        if (!out || i >= cap)
            break;
        out[i++] = id;
    }
    return list.size();
}

void sessQuickLootReplaceLists(int list_type, const uint16_t* blacklist, size_t blacklist_count, const uint16_t* whitelist,
                               size_t whitelist_count)
{
    tibia::quickloot::TQuickLootBlackWhitelistStorage* storage = tibia::game::currentQuickLootBlackWhitelistStorage();
    if (!storage)
        return;
    std::set<uint16_t> black;
    std::set<uint16_t> white;
    for (size_t i = 0; blacklist && i < blacklist_count; ++i)
        black.insert(blacklist[i]);
    for (size_t i = 0; whitelist && i < whitelist_count; ++i)
        white.insert(whitelist[i]);
    storage->replaceLists(list_type, std::move(black), std::move(white));
}

void sessImbuingAddOwned(uint16_t type_id, uint32_t count)
{
    tibia::game::TGameSessionBase* session = liveSession();
    if (session && session->imbuingStorage())
        session->imbuingStorage()->addOwned(type_id, count);
}

size_t sessImbuingSelectedAstralSources(QtClientAstralSource* out, size_t cap)
{
    const HostObjects* host = hostObjects();
    if (!host || !host->imbuingDialog)
        return 0;
    const auto* selected = qobject_cast<cip::tibia::imbuing::TQmlImbuementData*>(host->imbuingDialog->selectedImbuement());
    if (!selected)
        return 0;
    size_t total = 0;
    for (QObject* object : selected->astralSources()) {
        const auto* source = qobject_cast<cip::tibia::imbuing::TQmlAstralSource*>(object);
        if (!source)
            continue;
        if (out && total < cap) {
            out[total] = QtClientAstralSource{};
            out[total].type_id = static_cast<uint16_t>(source->objectID());
            out[total].count = source->objectCount();
            copyUtf8(out[total].name, sizeof out[total].name, source->name());
        }
        ++total;
    }
    return total;
}

int sessNpcTradeOption(const char* name_utf8)
{
    if (!name_utf8)
        return -1;
    const tibia::gamewindow::TNPCTradeOptions options = tibia::gamewindow::TNPCTradeOptions::load();
    const std::string_view name(name_utf8);
    if (name == "sellEquipped")
        return options.sellEquipped ? 1 : 0;
    if (name == "ignoreCapacity")
        return options.ignoreCapacity ? 1 : 0;
    if (name == "buyWithBackpacks")
        return options.buyWithBackpacks ? 1 : 0;
    if (name == "showNameFilter")
        return options.showNameFilter ? 1 : 0;
    if (name == "allowLargeTradesWithoutWarning")
        return options.allowLargeTradesWithoutWarning ? 1 : 0;
    return -1;
}

void sessSetAttackMark(uint32_t creature_id)
{
    if (tibia::creatures::TCreatureStorage* storage = tibia::game::currentCreatureStorage())
        storage->setAttackID(creature_id);
}

const QtClientSessionApiV1 g_sessionApi = {
    .is_online = &sessIsOnline,
    .is_dead = &sessIsDead,
    .local_player_present = &sessPlayerPresent,
    .local_player_map_pos = &sessPlayerMapPos,
    .local_player_name = &sessPlayerName,
    .local_player_id = &sessPlayerId,
    .local_player_level = &sessPlayerLevel,
    .local_player_stop_auto_walk = &sessStopWalk,
    .local_player_is_auto_walking = &sessIsWalking,
    .local_player_auto_walk = &sessAutoWalk,
    .talk = &sessTalk,
    .turn = &sessTurn,
    .millis = &sessMillis,
    .map_sea_floor = &sessSeaFloor,
    .world_light_intensity = &sessWorldLight,
    .map_item_revision = &sessItemRev,

    .local_player_has_reached_main = &sessHasReachedMain,
    .local_player_inventory_type_id = &sessInvTypeId,
    .local_player_inventory_slot = &sessInvSlot,
    .open_local_player_inventory = &sessOpenInv,

    .local_player_bless_visual_state = &sessBlessVisual,

    .local_player_health = &sessLpHealth,
    .local_player_max_health = &sessLpMaxHealth,
    .local_player_mana = &sessLpMana,
    .local_player_max_mana = &sessLpMaxMana,
    .local_player_level_percent = &sessLpLevelPct,
    .local_player_stamina = &sessLpStamina,
    .local_player_vocation = &sessLpVocation,
    .local_player_shield = &sessLpShield,
    .local_player_total_money = &sessLpMoney,
    .local_player_free_capacity = &sessLpFreeCap,
    .local_player_inventory_count = &sessLpInvCount,
    .local_player_outfit = &sessLpOutfit,

    .item_name = &sessItemName,
    .item_is_container = &sessItemIsContainer,
    .item_is_pickupable = &sessItemIsPickupable,
    .item_mean_price = &sessItemMeanPrice,
    .item_max_npc_sale_gold_price = &sessItemMaxNpcSale,

    .race_name = &sessRaceName,
    .race_exists = &sessRaceExists,
    .race_is_boss = &sessRaceIsBoss,
    .race_outfit = &sessRaceOutfitAbi,
    .race_ids_by_name = &sessRaceIdsByName,
    .all_race_ids = &sessAllRaceIds,

    .quest_info = &sessQuestInfoAbi,
    .all_quest_infos = &sessAllQuestInfos,

    .creature_at = &sessCreatureAt,
    .creature_at_id = &sessCreatureAtId,
    .spectators_near = &sessSpectatorsNear,
    .aware_range = &sessAwareRange,

    .item_hits = &sessItemHits,
    .top_use_hit = &sessTopUseHit,
    .find_items_in_range = &sessFindItemsInRange,
    .find_items_in_range_types = &sessFindItemsInRangeTypes,

    .is_sight_clear = &sessSightClear,
    .is_walkable = &sessWalkable,

    .safe_logout = &sessSafeLogout,
    .request_bless = &sessRequestBless,
    .party_join = &sessPartyJoin,
    .set_attacking_creature_id = &sessSetAttacking,
    .attacking_creature_id = &sessAttackingId,

    .character_name = &sessCharacterName,
    .is_switching_character = &sessIsSwitchingChar,

    .request_quick_loot_black_white_list = &sessQuickLootBw,
    .open_container_quick_loot = &sessOpenQuickLoot,

    .send_status_tracker_bestiary = &sessStatusBestiary,
    .request_bestiary_search = &sessBestiarySearch,

    .open_private_channel = &sessOpenPrivate,

    .request_team_finder_list = &sessTfList,
    .request_team_finder_leader = &sessTfLeader,
    .join_team_finder = &sessTfJoin,
    .leave_team_finder = &sessTfLeave,
    .create_team_finder = &sessTfCreate,
    .reset_team_finder_leader = &sessTfResetLeader,
    .update_team_finder_member = &sessTfUpdateMember,

    .use_inventory_item_on_creature = &sessUseInvOnCreature,
    .use_map_object = &sessUseMapObject,
    .use_inventory_item_with_map_object = &sessUseInvWithMap,


    .creature_type_is_summon = &sessCreatureTypeIsSummon,

    .find_player_item = &sessFindPlayerItem,


    .local_player_direction = &sessLpDirection,
    .local_player_hold_direction = &sessHoldDirection,
    .local_player_release_direction = &sessReleaseDirection,

    .character_configuration_loaded = &sessCharacterConfigurationLoaded,

    .team_finder_params = &sessTfParams,
    .team_finder_members = &sessTfMembers,
    .team_finder_teams = &sessTfTeams,
    .set_creature_hidden = &sessSetCreatureHidden,
    .stored_fields_on_floor = &sessStoredFieldsOnFloor,
    .is_aware_of_position = &sessIsAwareOfPosition,
    .waypoint_costs_snapshot = &sessWaypointCostsSnapshot,
    .waypoint_cost_at = &sessWaypointCostAt,
    .waypoint_costs_release = &sessWaypointCostsRelease,
    .static_map_subarea_at = &sessStaticMapSubAreaAt,
    .static_map_area = &sessStaticMapArea,
    .static_map_area_ids = &sessStaticMapAreaIds,
    .static_map_subareas_of_area = &sessStaticMapSubAreasOfArea,

    .open_container_ids = &sessOpenContainerIds,
    .container_info = &sessContainerInfo,
    .container_items = &sessContainerItems,
    .quick_loot_list_type = &sessQuickLootListType,
    .quick_loot_list = &sessQuickLootList,
    .quick_loot_replace_lists = &sessQuickLootReplaceLists,
    .imbuing_add_owned = &sessImbuingAddOwned,
    .imbuing_selected_astral_sources = &sessImbuingSelectedAstralSources,
    .npc_trade_option = &sessNpcTradeOption,
    .set_attack_mark = &sessSetAttackMark,
};

} // namespace

const QtClientSessionApiV1* sessionApiV1()
{
    return &g_sessionApi;
}

} // namespace tibia::mods
