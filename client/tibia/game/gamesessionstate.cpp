#include "game/gamesessionbase.h"
#include "game/currentgamesession.h"
#include <stdexcept>
#include "game/gamesessionstate.h"
#include "game/gameactionrouter.h"
#include "tibia/runtime/host.h"
#include <tibia/game/sessionhooks.h>
#include "tibia/gamewindow/remainingdialogs.h"

#include <tibia/game/gamenotify.h>

#include <tibia/creatures/attachedeffect.h>
#include <tibia/config/runtimegameconfig.h>
#include "input/playermovementgameactionhandler.h"
#include "input/playermovementintenthandler.h"
#include "mods/registry.h"
#include "input/movementpathplanner.h"
#include "qtclient/abi/const.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "minimap/waypointcosts.h"
#include <tibia/game/mapsession.h>
#include "protocol/clientmessages.h"
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"
#include <tibia/appearances/appearancetype.h>
#include "tibia/assets/things.h"
#include "worldmap/worldmapfield.h"
#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "game/playerdata.h"
#include "game/gamesessionmetainfo.h"
#include "clockservice.h"
#include <QStringList>
#include <tibia/schedule/schedule.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/mapsession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/game/tgamenotify.h>

#include <QString>
#include <tibia/game/thingsession_forward.h>

namespace tibia::game {

namespace gameclient = ::tibia::protocol::gameclient;

TGameSessionState& gameSessionState()
{
    static TGameSessionState state;
    return state;
}

TGameSessionState::~TGameSessionState() = default;

namespace {

void bindExtendedOpcodeSender(const bool bound)
{
    if (!bound) {
        tibia::protocol::ExtendedOpcodeReader::instance().setSender(nullptr);
        return;
    }
    tibia::protocol::ExtendedOpcodeReader::instance().setSender([](const uint8_t opcode, const std::string& buffer) {
        const auto* connection = tibia::network::TGameserverSessionNetwork::live();
        if (!connection || !connection->isConnected())
            return false;
        gameclient::sendExtendedOpcode(opcode, buffer);
        return true;
    });
}

std::shared_ptr<tibia::network::TGameserverSessionNetwork> newGameConnection()
{
    // Retired (not deleted) on release: the release can run inside the
    // network's own failure callback.
    return { new tibia::network::TGameserverSessionNetwork(),
             [](tibia::network::TGameserverSessionNetwork* connection) { connection->retire(); } };
}

}

void TGameSessionState::terminate()
{
    resetGameStates();
    bindExtendedOpcodeSender(false);
    m_gameConnection = nullptr;
}

tibia::input::TPlayerMovementIntentHandler* TGameSessionState::movementIntent()
{
    auto* session = dynamic_cast<TGameserverGameSession*>(currentGameSession());
    return session ? session->playerMovementIntentHandler().get() : nullptr;
}

tibia::input::TPlayerMovementIntentHandler::Environment TGameSessionState::playerMovementIntentEnvironment()
{
    tibia::input::TPlayerMovementIntentHandler::Environment env;
    env.isInView = [](const tibia::worldmap::TWorldMapCoordinate& coordinate) {
        return tibia::game::isAwareOf({ coordinate.x, coordinate.y, coordinate.z });
    };
    env.claimUnreachable = [](const tibia::worldmap::TWorldMapCoordinate& goal, const int result) {
        int reason = 0;
        switch (result) {
        case tibia::input::PathNoWay: reason = QTCLIENT_WALK_UNREACHABLE_NO_WAY; break;
        case tibia::input::PathOutOfRange: reason = QTCLIENT_WALK_UNREACHABLE_OUT_OF_RANGE; break;
        case tibia::input::PathGoDownstairs: reason = QTCLIENT_WALK_UNREACHABLE_GO_DOWNSTAIRS; break;
        case tibia::input::PathGoUpstairs: reason = QTCLIENT_WALK_UNREACHABLE_GO_UPSTAIRS; break;
        default: return false;
        }
        return tibia::mods::claimWalkUnreachable(goal.x, goal.y, goal.z, reason);
    };
    return env;
}

void TGameSessionState::resetGameStates()
{
    m_online = false;
    m_dead = false;
    m_sceneFrozen = false;
    m_serverBeat = 50;
    m_seq = 0;
    m_mapUpdatedAt = 0;
    m_mapUpdateTimer = { true, tibia::shared::TFrameStopWatch{} };
    setCanReportBugs(false);
    setBugReportState(-1);
    setCanExivaOptions(false);
    m_followingCreatureId = 0;
    m_attackingCreatureId = 0;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        storage->setAttackID(0);
        storage->setFollowID(0);
    }
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->reset();
    m_playerPresent = false;
    if (TGameSessionBase* session = currentGameSession())
        session->objectCountStorage()->clear();
    if (m_checkConnectionEvent) {
        m_checkConnectionEvent->cancel();
        m_checkConnectionEvent = nullptr;
    }

    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->clear();
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance())
        inventory->clear();
    tibia::game::resetAwareRange();
}

void TGameSessionState::processConnectionError(const std::string_view message, const bool clean)
{
    // Errors only matter while the session still has its connection.
    if (m_gameConnection) {
        if (!clean)
            tibia::game::notify("onConnectionError", std::string(message), 1);
        processDisconnect();
    }
}

void TGameSessionState::processDisconnect()
{
    if (isOnline() || m_switchingCharacter) {
        m_switchingCharacter = false;
        processGameEnd();
    }

    if (m_gameConnection) {
        m_gameConnection->disconnect();
        bindExtendedOpcodeSender(false);
        m_gameConnection = nullptr;
    }
}










void TGameSessionState::beginPlayerSession(const std::string_view characterName)
{
    m_playerPresent = true;
    // The login names the own creature.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
        storage->setPlayerID(0);
    if (tibia::game::TPlayerData* data = tibia::game::TPlayerData::liveInstance())
        data->setCharacterName(characterName);
    tibia::worldmap::TWorldMapStorage::ensureStorage()->sessionState().resetLocalPlayer();
}

void TGameSessionState::processEnterGame()
{
    tibia::game::notify("onEnterGame");
}


void TGameSessionState::processGameStart()
{
    m_online = true;
    tibia::host::app().resetTargetFps();

    m_checkConnectionEvent = tibia::schedule::every([this] {
        if (!isConnectionOk() && !m_connectionFailWarned) {
            tibia::game::notify("onConnectionFailing", true);
            tibia::game::TGameNotify::instance()->emitConnectionFailing(true);
            m_connectionFailWarned = true;
        } else if (isConnectionOk() && m_connectionFailWarned) {
            tibia::game::notify("onConnectionFailing", false);
            tibia::game::TGameNotify::instance()->emitConnectionFailing(false);
            m_connectionFailWarned = false;
        }
    }, 1000);

    tibia::game::notify("onGameStart");
    tibia::game::TGameNotify::instance()->emitGameStarted();

    if (m_switchingCharacter) {
        m_switchingCharacter = false;
        tibia::game::notify("onCharacterSwitched", m_characterName);
        tibia::game::TGameNotify::instance()->emitCharacterSwitched(QString::fromStdString(m_characterName));
    }
}

void TGameSessionState::processCharacterSwitch(const std::string_view characterName)
{
    // CipSoft session end: modules reset on onGameEnd, but the socket stays open and
    // logout-only UI (character list, background) checks isSwitchingCharacter.
    m_switchingCharacter = true;
    endSession();

    resetGameStates();
    tibia::game::cleanMapDynamicThings();

    beginPlayerSession(characterName);
    m_characterName = characterName;
}

void TGameSessionState::endSession()
{
    m_online = false;
    tibia::game::notify("onGameEnd");
    tibia::game::TGameNotify::instance()->emitGameEnded();

    if (m_connectionFailWarned) {
        tibia::game::notify("onConnectionFailing", false);
        tibia::game::TGameNotify::instance()->emitConnectionFailing(false);
        m_connectionFailWarned = false;
    }
}

void TGameSessionState::processGameEnd()
{
    // FPS fixed at 60 for when UI is rendering alone.
    tibia::host::app().setTargetFps(60u);

    endSession();

    // reset game state
    resetGameStates();

    m_worldName = "";
    m_characterName = "";

    // clean map creatures
    tibia::game::cleanMapDynamicThings();
}


void TGameSessionState::setSceneFrozen(const bool frozen)
{
    if (m_sceneFrozen == frozen)
        return;

    m_sceneFrozen = frozen;
}














void TGameSessionState::processOpenContainer(const uint8_t containerId, const tibia::container::TInventorySlot& containerItem,
                                const std::string_view name, const uint8_t capacity, const bool hasParent,
                                const std::vector<tibia::container::TInventorySlot>& items, const bool isUnlocked,
                                const bool hasPages, const bool showSearch, const uint16_t containerSize,
                                const uint16_t firstIndex)
{
    tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance();
    if (!storage)
        return;

    // Replace-open: applyOpen emits containerUpdated(id, true); UI resets in place
    // (no separate remove). Matches former onOpen-then-previous-onClose ordering.
    storage->applyOpen(containerId, capacity, QString::fromUtf8(name.data(), static_cast<int>(name.size())),
                       containerItem, hasParent, isUnlocked, hasPages, showSearch, containerSize, firstIndex, items);
}


void TGameSessionState::processContainerAddItem(const uint8_t containerId, const tibia::container::TInventorySlot& item,
                                   const uint16_t slot)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyAddItem(containerId, item, slot);
}

void TGameSessionState::processContainerUpdateItem(const uint8_t containerId, const uint16_t slot,
                                      const tibia::container::TInventorySlot& item)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyUpdateItem(containerId, slot, item);
}

void TGameSessionState::processContainerRemoveItem(const uint8_t containerId, const uint16_t slot,
                                      const tibia::container::TInventorySlot& lastItem)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyRemoveItem(containerId, slot, lastItem);
}

void TGameSessionState::processInventoryChange(const uint8_t slot, const tibia::container::TInventorySlot& item)
{
    tibia::container::TInventorySlot row = item;
    if (!row.empty() && row.durationSeconds > 0) {
        TAppearanceType* type = row.appearance ? row.appearance->thingType()
                                         : tibia::game::rawThingType(row.typeId, ThingCategoryItem);
        // expirestop-only items are paused server-side when equipped in their cloth slot.
        if (type && type->getClothSlot() == static_cast<int>(slot)
            && type->hasExpire()) {
            row.decaying = true;
            row.durationEndMs = tibia::shared::TClockService::frameMillis() + static_cast<int64_t>(row.durationSeconds) * 1000;
        }
    }

    // The inventory container holds the slots.
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        if (!row.empty())
            inventory->setSlot(slot, row);
        else
            inventory->clearSlot(slot);
    }

    tibia::game::notify("onInventoryChange", static_cast<tibia::client::InventorySlot>(slot));
}
















void TGameSessionState::processOwnTrade(const std::string_view name, const std::vector<tibia::container::TInventorySlot>& items)
{
    tibia::game::notify("onOwnTrade", name, items);
}

void TGameSessionState::processCounterTrade(const std::string_view name, const std::vector<tibia::container::TInventorySlot>& items)
{
    tibia::game::notify("onCounterTrade", name, items);
}






void TGameSessionState::processModalDialog(const uint32_t id, const std::string_view title, const std::string_view message, const std::vector<std::tuple<uint8_t, std::string>>
                                & buttonList, const uint8_t enterButton, const uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>
                                & choiceList, const bool priority)
{
    tibia::game::notify("onModalDialog", id, title, message, buttonList, enterButton, escapeButton, choiceList, priority);
}

















void TGameSessionState::processUpdateTarget(const uint32_t seq, const uint32_t targetId)
{
    // CIP UpdateTarget: the server's current attack target. It only updates the
    // local target; answering with an Attack message would echo the server.
    if (seq != 0 && seq != m_seq)
        return;
    const bool cancelled = targetId == 0 && isAttacking();
    setAttackingCreatureId(targetId);
    if (cancelled)
        tibia::game::notify("onAttackCancel");
}


void TGameSessionState::loginWorld(const std::string_view worldName, const std::string_view characterName,
                                   std::shared_ptr<tibia::network::TGameserverSessionNetwork> connection)
{
    if (m_gameConnection || isOnline())
        throw std::runtime_error("Unable to login into a world while already online or logging.");

    if (m_protocolVersion == 0)
        throw std::runtime_error("Must set a valid game protocol version before logging.");

    // reset the new game state
    resetGameStates();

    beginPlayerSession(characterName);

    m_characterName = characterName;
    m_worldName = worldName;
    m_gameConnection = std::move(connection);
    bindExtendedOpcodeSender(true);
}

void TGameSessionState::startDecodedReplay()
{
    if (m_gameConnection || isOnline())
        throw std::runtime_error("Unable to login into a world while already online or logging.");

    if (m_protocolVersion == 0)
        throw std::runtime_error("Must set a valid game protocol version before logging.");

    resetGameStates();

    beginPlayerSession("Player");

    m_characterName = "Player";
    m_worldName = "Record";
    m_gameConnection = newGameConnection();
    bindExtendedOpcodeSender(true);
    m_gameConnection->beginReplay();
}

void TGameSessionState::deliverReplayPacket(const uint8_t* data, const size_t size)
{
    if (!m_gameConnection || !data || size == 0 || size > 0xffff)
        return;
    m_gameConnection->deliverPacket(data, size);
}

void TGameSessionState::finishDecodedReplay()
{
    if (m_gameConnection)
        m_gameConnection->fail({}, true);
}

void TGameSessionState::cancelLogin()
{
    // send logout even if the game has not started yet, to make sure that the player doesn't stay logged there
    if (m_gameConnection)
        gameclient::sendLogout();

    processDisconnect();
}

void TGameSessionState::forceLogout()
{
    if (!isOnline())
        return;

    gameclient::sendLogout();
    processDisconnect();
}


bool TGameSessionState::walk(const tibia::client::Direction direction)
{
    tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    if (!canPerformGameAction() || direction == tibia::client::InvalidDirection || !intent)
        return false;
    tibia::game::notify("onWalk", direction);
    if (m_mapUpdateTimer.first) {
        m_mapUpdateTimer.second.restart();
        m_mapUpdateTimer.first = false;
    }
    intent->step(tibia::input::toMovementDirection(direction));
    return true;
}

void TGameSessionState::requestStop()
{
    if (!canPerformGameAction() || !m_gameConnection)
        return;
    gameclient::sendStop();
}

void TGameSessionState::interruptAutoWalk()
{
    if (!m_playerPresent)
        return;
    const tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    const bool hadPath = intent && intent->hasGoal();
    tibia::game::localPlayerStopAutoWalk();
    if (isFollowing())
        cancelFollow();
    if (hadPath)
        requestStop();
}



void TGameSessionState::lookCreature(const uint32_t creatureId, const bool isBattleList)
{
    if (!canPerformGameAction() || !creatureId)
        return;

    if (isBattleList && m_protocolVersion >= 961) {
        gameclient::sendLookCreature(creatureId);
        return;
    }

    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> row =
        storage ? storage->creatureAt(creatureId) : nullptr;
    if (!row)
        return;

    int stackPos = 0;
    if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(row->position())) {
        for (const auto& entry : tile->stackCreatureEntries()) {
            if (entry.creatureId == creatureId) {
                stackPos = entry.stackIndex;
                break;
            }
        }
    }
    gameclient::sendLook(row->position(), static_cast<int>(Proto::Creature), static_cast<uint8_t>(stackPos));
}










void TGameSessionState::useOnCreature(const TObjectReference& object, const uint32_t creatureId)
{
    if (!canPerformGameAction() || object.typeId == 0 || !creatureId)
        return;

    Position pos = object.position;
    if (!pos.isValid())
        pos = Position(0xFFFF, 0, 0);

    const auto stackPos = static_cast<uint8_t>(std::max(0, object.stackPos));
    gameclient::sendUseOnCreature(pos, object.typeId, stackPos, creatureId);
    tibia::game::notify("onUseWith", pos, static_cast<uint32_t>(object.typeId), creatureId, static_cast<int>(stackPos));
}



std::optional<TObjectReference> TGameSessionState::findItemInContainers(const uint32_t itemId, const int subType, const uint8_t tier)
{
    tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance();
    if (!storage)
        return std::nullopt;

    for (const tibia::container::TContainerID containerId : storage->openIds()) {
        const tibia::container::TStoredContainer* row = storage->containerAt(containerId);
        if (!row)
            continue;
        for (size_t i = 0; i < row->items().size(); ++i) {
            const tibia::container::TInventorySlot& slot = row->items()[i];
            if (slot.empty() || slot.typeId != itemId || slot.tier != tier)
                continue;
            const uint8_t fluidOrSub = slot.fluidOrSplash ? slot.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            const int pageSlot = static_cast<int>(i);
            return TObjectReference{ slot.typeId,
                                     Position(0xffff, static_cast<uint16_t>(containerId | 0x40), static_cast<uint8_t>(pageSlot)),
                                     pageSlot, static_cast<uint16_t>(slot.stackable ? slot.count : 0), slot.tier, fluidOrSub };
        }
    }

    return std::nullopt;
}





void TGameSessionState::attack(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    if (creatureId && creatureId == tibia::game::localPlayerId())
        return;

    // Attacking the target again stops (CIP's providers pick stop then).
    uint32_t id = creatureId;
    if (id && id == m_attackingCreatureId)
        id = 0;
    // CIP: stopping sends only while there is a target; attacking leaves the
    // follow target and any walk alone. The sequence is the creature id (0 to stop).
    if (!id && !m_attackingCreatureId)
        return;

    setAttackingCreatureId(id);
    // The creature storage holds the targets and their frames; a new attack
    // target drops the follow target.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        storage->setAttackID(id);
        setFollowingCreatureId(storage->followID());
    }
    if (m_protocolVersion >= 963)
        m_seq = id;
    else
        ++m_seq;

    gameclient::sendAttack(id, m_seq);

    // Attack requests only; setAttackingCreatureId also mirrors server-driven targets.
    tibia::game::notify("onAttack", id);
}

void TGameSessionState::follow(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    if (creatureId && creatureId == tibia::game::localPlayerId())
        return;

    // Following the target again stops; same rules as attack.
    uint32_t id = creatureId;
    if (id && id == m_followingCreatureId)
        id = 0;
    if (!id && !m_followingCreatureId)
        return;

    setFollowingCreatureId(id);
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        storage->setFollowID(id);
        setAttackingCreatureId(storage->attackID());
    }
    if (m_protocolVersion >= 963)
        m_seq = id;
    else
        ++m_seq;

    gameclient::sendFollow(id, m_seq);
}








void TGameSessionState::sendPartyAnalyzerAction(const uint8_t action, const std::vector<std::tuple<uint16_t, uint64_t>>& items)
{
    if (!canPerformGameAction())
        return;

    gameclient::sendPartyAnalyzerAction(action, items);
}















void TGameSessionState::changeOutfit(const Outfit& outfit)
{
    changeOutfitAndMount(outfit, outfit.hasMount(), false);
}

void TGameSessionState::changeOutfitAndMount(const Outfit& outfit, const bool mounted, const bool randomMount)
{
    if (!canPerformGameAction())
        return;

    gameclient::sendChangeOutfit(outfit, mounted, randomMount);
}
























bool TGameSessionState::isAttacking()
{
    if (!m_attackingCreatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    return storage && !!storage->creatureAt(m_attackingCreatureId);
}
bool TGameSessionState::isFollowing()
{
    if (!m_followingCreatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    return storage && !!storage->creatureAt(m_followingCreatureId);
}
bool TGameSessionState::isConnectionOk() { return m_gameConnection && m_gameConnection->millisSinceRead() < 5000; }

bool TGameSessionState::canPerformGameAction() const
{
    // we can only perform game actions if we meet these conditions:
    // - the game is online
    // - the local player exists
    // - the local player is not dead
    // - we have a game protocol
    // - the game protocol is connected
    return m_online && m_playerPresent && !m_dead && m_gameConnection && m_gameConnection->isConnected();
}

void TGameSessionState::setProtocolVersion(const uint16_t version)
{
    if (m_protocolVersion == version)
        return;

    if (isOnline())
        throw std::runtime_error("Unable to change protocol version while online");

    if (version != 0 && (version < 740 || version > tibia::config::lastSupportedVersion()))
        throw std::runtime_error(std::format("Protocol version {} not supported", version));

    m_protocolVersion = version;

    Proto::buildMessageModesMap(version);

    tibia::game::notify("onProtocolVersionChange", version);
}

void TGameSessionState::setClientVersion(const uint16_t version)
{
    if (m_clientVersion == version)
        return;

    if (isOnline())
        throw std::runtime_error("Unable to change client version while online");

    if (version != 0 && (version < 740 || version > tibia::config::lastSupportedVersion()))
        throw std::runtime_error(std::format("Client version {} not supported", version));


    m_clientVersion = version;

    tibia::game::notify("onClientVersionChange", version);
}

void TGameSessionState::setAttackingCreatureId(const uint32_t creatureId)
{
    if (creatureId == m_attackingCreatureId)
        return;

    const uint32_t oldId = m_attackingCreatureId;
    m_attackingCreatureId = creatureId;

    tibia::game::notify("onAttackingCreatureChange", creatureId, oldId);
}

void TGameSessionState::setFollowingCreatureId(const uint32_t creatureId)
{
    if (creatureId == m_followingCreatureId)
        return;

    const uint32_t oldId = m_followingCreatureId;
    m_followingCreatureId = creatureId;

    tibia::game::notify("onFollowingCreatureChange", creatureId, oldId);
}


int TGameSessionState::findEmptyContainerId()
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        return storage->findEmptyId();
    return 0;
}



















void TGameSessionState::imbuementDurations(const bool isOpen)
{
    if (!canPerformGameAction())
        return;

    gameclient::sendImbuementDurations(isOpen);
}




void TGameSessionState::requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t /*size*/, const std::vector<uint16_t>& listedItems)
{
    if (!canPerformGameAction())
        return;

    // Always trust the vector length — Lua `#` can disagree with table contents and
    // a mismatched size desyncs the game protocol (server reads the wrong byte count).
    const uint16_t actualSize = static_cast<uint16_t>(listedItems.size());
    gameclient::requestQuickLootBlackWhiteList(filter, actualSize, listedItems);
}
















void TGameSessionState::sendStoreEvent(const uint8_t eventType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendStoreEvent(eventType);
}

void TGameSessionState::sendClientCheck(const std::vector<uint8_t>& data)
{
    if (!m_gameConnection)
        return;
    gameclient::sendClientCheck(data);
}


void TGameSessionState::sendClientDetails(const std::string_view details)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendClientDetails(details);
}

}
