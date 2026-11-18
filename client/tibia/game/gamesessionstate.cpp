#include "game/gamesessionbase.h"
#include <stdexcept>
#include "game/gamesessionstate.h"
#include "game/gameactionrouter.h"
#include "tibia/runtime/host.h"
#include <tibia/game/sessionhooks.h>
#include "tibia/gamewindow/remainingdialogs.h"

#include <tibia/game/gamenotify.h>

#include <tibia/creatures/attachedeffect.h>
#include <tibia/config/runtimegameconfig.h>
#include "input/playermovementintenthandler.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "app/clientoptions.h"
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
#include "creatures/walkdebug.h"
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
        const auto* connection = tibia::network::TGameserverNetworkPacketProcessor::live();
        if (!connection || !connection->isConnected())
            return false;
        gameclient::sendExtendedOpcode(opcode, buffer);
        return true;
    });
}

std::shared_ptr<tibia::network::TGameserverNetworkPacketProcessor> newGameConnection()
{
    // Retired (not deleted) on release: the release can run inside the
    // processor's own failure callback.
    return { new tibia::network::TGameserverNetworkPacketProcessor(),
             [](tibia::network::TGameserverNetworkPacketProcessor* connection) { connection->retire(); } };
}

}

void TGameSessionState::terminate()
{
    resetGameStates();
    bindExtendedOpcodeSender(false);
    m_gameConnection = nullptr;
}

namespace {

struct TMovementOptions
{
    bool alwaysTurnTowardsMoveDirection = false;
    bool autoChaseEnabled = false;
    int keyboardDelayMs = 250;

    void load()
    {
        const QJsonObject options = tibia::app::ClientOptions::load().options();
        const QJsonObject& defaults = tibia::config::defaultClientOptions();
        const auto value = [&](const char* name) {
            const QLatin1String key(name);
            return options.contains(key) ? options.value(key) : defaults.value(key);
        };
        alwaysTurnTowardsMoveDirection = value("alwaysTurnTowardsMoveDirection").toBool();
        autoChaseEnabled = value("autoChaseEnabled").toBool();
        keyboardDelayMs = value("keyboardDelayUseDefault").toBool(true) ? 250 : value("keyboardDelayMs").toInt(250);
    }
};

/// A field the planner avoids: not free (blocked or unsure) or marked avoid.
bool isAvoidedField(tibia::worldmap::TWorldMapField& field)
{
    return field.walkState() != 0 || !field.isPathable();
}

/// Minimap costs, overridden by the fields the client sees: an avoided field or
/// one with an unpassable item is blocked, else the first ground item's step
/// cost (blocked when there is none), at most 250.
tibia::input::WaypointCost snapshotWaypointCost()
{
    std::shared_ptr<tibia::minimap::TWaypointCosts> minimap = tibia::game::waypointCosts();
    return [minimap](const int x, const int y, const int z) -> quint8 {
        tibia::worldmap::TWorldMapStorage* map = tibia::worldmap::TWorldMapStorage::ensureStorage();
        if (tibia::worldmap::TWorldMapField* field = map->fieldAt(tibia::worldmap::TWorldMapCoordinate(x, y, z))) {
            if (isAvoidedField(*field) || !field->isWalkable(true))
                return tibia::minimap::TWaypointCosts::BlockedCost;
            if (const int cost = field->movementGroundSpeed(); cost > 0)
                return static_cast<quint8>(std::min(cost, 250));
            return minimap ? minimap->costAt(x, y, z) : tibia::minimap::TWaypointCosts::UnknownCost;
        }
        // The server sends nothing for a field without ground, so an aware coordinate
        // with no field is void, as the minimap records it once the area is seen.
        if (tibia::game::isAwareOf({ x, y, z }))
            return tibia::minimap::TWaypointCosts::BlockedCost;
        return minimap ? minimap->costAt(x, y, z) : tibia::minimap::TWaypointCosts::UnknownCost;
    };
}

/// The avoided fields in view on the player's floor.
std::vector<tibia::worldmap::TWorldMapCoordinate> avoidedFieldsInView()
{
    std::vector<tibia::worldmap::TWorldMapCoordinate> fields;
    const Position center = tibia::game::localPlayerPosition();
    if (!center.isValid())
        return fields;
    tibia::worldmap::TWorldMapStorage* map = tibia::worldmap::TWorldMapStorage::ensureStorage();
    for (int dy = -8; dy <= 8; ++dy) {
        for (int dx = -10; dx <= 10; ++dx) {
            const tibia::worldmap::TWorldMapCoordinate c(center.x + dx, center.y + dy, center.z);
            if (!tibia::game::isAwareOf({ c.x, c.y, c.z }))
                continue;
            if (tibia::worldmap::TWorldMapField* field = map->fieldAt(c); field && isAvoidedField(*field))
                fields.push_back(c);
        }
    }
    return fields;
}

}

void TGameSessionState::createMovementIntent()
{
    using tibia::input::TPlayerMovementIntentHandler;
    m_movementIntent.reset();
    if (!m_playerPresent)
        return;
    auto options = std::make_shared<TMovementOptions>();
    options->load();
    TPlayerMovementIntentHandler::Environment env;
    env.now = [] { return static_cast<qint64>(tibia::shared::TClockService::frameMillis()); };
    env.player = [this]() -> tibia::creatures::TCreature* {
        tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
        if (!storage || !m_playerPresent)
            return nullptr;
        const auto row = storage->player();
        return row && row->position().isValid() ? row.get() : nullptr;
    };
    env.map = [] { return tibia::worldmap::TWorldMapStorage::ensureStorage(); };
    env.waypointCost = snapshotWaypointCost;
    env.blockedFields = avoidedFieldsInView;
    env.alwaysTurnTowardsMoveDirection = [options] { return options->alwaysTurnTowardsMoveDirection; };
    env.keyboardDelayMs = [options] { return options->keyboardDelayMs; };
    env.stopChaseAndFollow = [this, options] {
        if (!options->autoChaseEnabled && isAttacking() && m_chaseMode == tibia::client::ChaseOpponent)
            setChaseMode(tibia::client::DontChase);
        if (isFollowing()) {
            Q_EMIT m_movementIntent->publishGameAction(
                std::make_shared<tibia::input::TGameActionCreature>(tibia::input::EGameAction::STOP_FOLLOWING_CREATURE, 0));
            Q_EMIT m_movementIntent->publishGameAction(
                std::make_shared<tibia::input::TGameActionCreature>(tibia::input::EGameAction::STOP_ATTACKING_CREATURE, 0));
        }
    };
    env.stopFollowing = [this] {
        Q_EMIT m_movementIntent->publishGameAction(
            std::make_shared<tibia::input::TGameActionCreature>(tibia::input::EGameAction::STOP_FOLLOWING_CREATURE, 0));
    };
    env.isInView = [](const tibia::worldmap::TWorldMapCoordinate& coordinate) {
        return tibia::game::isAwareOf({ coordinate.x, coordinate.y, coordinate.z });
    };
    env.playerStates = [] { return static_cast<quint64>(tibia::game::localPlayerStates()); };
    env.showFailure = [this](const QString& textId) {
        Q_EMIT m_movementIntent->publishGameAction(
            std::make_shared<tibia::input::TGameActionSetStatusLine>(qtTrId(textId.toLatin1().constData())));
    };
    m_movementIntent = std::make_unique<TPlayerMovementIntentHandler>(std::move(env));
    TPlayerMovementIntentHandler* intent = m_movementIntent.get();
    // CIP: the session's movement intent handler publishes through the session's router.
    if (TGameserverGameSession* session = TGameserverGameSession::liveInstance())
        QObject::connect(intent, &TPlayerMovementIntentHandler::publishGameAction, session->gameActionRouter()->internalRouter(),
                         &TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(tibia::app::ClientOptionsNotifier::instance(), &tibia::app::ClientOptionsNotifier::optionChanged, intent,
                     [options](const QString& name) {
                         if (name.startsWith(QLatin1String("keyboardDelay")) || name == QLatin1String("alwaysTurnTowardsMoveDirection")
                             || name == QLatin1String("autoChaseEnabled"))
                             options->load();
                     });
    const auto wire = [this](void (*send)()) {
        return [this, send] {
            if (canPerformGameAction())
                send();
        };
    };
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoNorth, intent, wire(&gameclient::sendWalkNorth));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoNorthEast, intent, wire(&gameclient::sendWalkNorthEast));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoEast, intent, wire(&gameclient::sendWalkEast));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoSouthEast, intent, wire(&gameclient::sendWalkSouthEast));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoSouth, intent, wire(&gameclient::sendWalkSouth));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoSouthWest, intent, wire(&gameclient::sendWalkSouthWest));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoWest, intent, wire(&gameclient::sendWalkWest));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoNorthWest, intent, wire(&gameclient::sendWalkNorthWest));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendRotateNorth, intent, wire(&gameclient::sendTurnNorth));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendRotateEast, intent, wire(&gameclient::sendTurnEast));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendRotateSouth, intent, wire(&gameclient::sendTurnSouth));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendRotateWest, intent, wire(&gameclient::sendTurnWest));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendStop, intent, wire(&gameclient::sendStop));
    QObject::connect(intent, &TPlayerMovementIntentHandler::sendGoPath, intent,
                     [this](const std::vector<tibia::creatures::EMovementDirection>& path) {
                         if (!canPerformGameAction() || !m_gameConnection)
                             return;
                         std::vector<tibia::client::Direction> dirs;
                         dirs.reserve(path.size());
                         for (const auto direction : path)
                             dirs.push_back(tibia::input::toClientDirection(direction));
                         tibia::game::notify("onAutoWalk", tibia::game::localPlayerId(), dirs);
                         gameclient::sendAutoWalk(dirs);
                     });
}

void TGameSessionState::resetGameStates()
{
    m_online = false;
    m_dead = false;
    m_sceneFrozen = false;
    m_serverBeat = 50;
    m_seq = 0;
    m_ping = -1;
    m_mapUpdatedAt = 0;
    m_mapUpdateTimer = { true, tibia::shared::TFrameStopWatch{} };
    setCanReportBugs(false);
    setBugReportState(-1);
    setCanExivaOptions(false);
    m_fightMode = tibia::client::FightBalanced;
    m_chaseMode = tibia::client::DontChase;
    m_pvpMode = tibia::client::WhiteDove;
    m_safeFight = true;
    m_followingCreatureId = 0;
    m_attackingCreatureId = 0;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        storage->setAttackID(0);
        storage->setFollowID(0);
    }
    m_movementIntent.reset();
    m_playerPresent = false;
    m_marketAvailable = false;
    m_supplyStashAvailable = false;
    if (TGameSessionBase* session = TGameSessionBase::active())
        session->objectCountStorage()->clear();
    m_pingSent = 0;
    m_pingReceived = 0;
    m_unjustifiedPoints = UnjustifiedPoints();

    if (m_pingEvent) {
        m_pingEvent->cancel();
        m_pingEvent = nullptr;
    }

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
    createMovementIntent();
}

void TGameSessionState::processPendingGame()
{
    tibia::game::notify("onPendingGame");
    gameclient::sendEnterGame();
}

void TGameSessionState::processEnterGame()
{
    tibia::game::notify("onEnterGame");
}


void TGameSessionState::processGameStart()
{
    m_online = true;
    tibia::host::app().resetTargetFps();

    // synchronize fight modes with the server
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);

    // An in-flight ping (kept across alt switch) reschedules the loop from processPingBack.
    if (m_pingSent == m_pingReceived) {
        m_pingEvent = tibia::schedule::after([this] { ping(); }, m_pingDelay);
    }

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

    // Same socket: a ping sent before the switch still gets its reply, so keep the counters paired.
    const auto pingSent = m_pingSent;
    const auto pingReceived = m_pingReceived;
    resetGameStates();
    m_pingSent = pingSent;
    m_pingReceived = pingReceived;
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
    tibia::world::notifySessionEnd();
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

void TGameSessionState::processReadyForSecondaryConnection(const std::string_view token)
{
    m_secondaryConnectionToken = std::string(token);
    tibia::game::notify("onReadyForSecondaryConnection", m_secondaryConnectionToken);
}








void TGameSessionState::processPlayerModes(const tibia::client::FightModes fightMode, const tibia::client::ChaseModes chaseMode, const bool safeMode, const tibia::client::PVPModes pvpMode)
{
    m_fightMode = fightMode;
    m_chaseMode = chaseMode;
    m_safeFight = safeMode;
    m_pvpMode = pvpMode;

    tibia::game::notify("onFightModeChange", fightMode);
    tibia::game::notify("onChaseModeChange", chaseMode);
    tibia::game::notify("onSafeFightChange", safeMode);
    tibia::game::notify("onPVPModeChange", pvpMode);
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






void TGameSessionState::processEditGuildMessage(const std::string_view text)
{
    tibia::game::notify("onEditGuildMessage", text);
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


void TGameSessionState::loginWorld(const std::string_view account, const std::string_view password, const std::string_view worldName, const std::string_view worldHost, const int worldPort, const std::string_view characterName, const std::string_view authenticatorToken, const std::string_view sessionKey, const std::string_view& recordTo)
{
    if (m_gameConnection || isOnline())
        throw std::runtime_error("Unable to login into a world while already online or logging.");

    if (m_protocolVersion == 0)
        throw std::runtime_error("Must set a valid game protocol version before logging.");

    // reset the new game state
    resetGameStates();

    beginPlayerSession(characterName);

    (void)account;
    (void)password;
    (void)authenticatorToken;
    m_characterName = characterName;
    m_worldName = worldName;
    m_gameConnection = newGameConnection();
    bindExtendedOpcodeSender(true);
    tibia::network::TGameserverLoginData login;
    login.sessionKey = sessionKey;
    login.characterName = characterName;
    login.host = worldHost;
    login.port = static_cast<uint16_t>(worldPort);
    login.recordTo = recordTo;
    m_gameConnection->connectToGameserver(login);
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
    if (!canPerformGameAction() || direction == tibia::client::InvalidDirection || !m_movementIntent)
        return false;
    tibia::game::notify("onWalk", direction);
    if (m_mapUpdateTimer.first) {
        m_mapUpdateTimer.second.restart();
        m_mapUpdateTimer.first = false;
    }
    m_movementIntent->step(tibia::input::toMovementDirection(direction));
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
    const bool hadPath = m_movementIntent && m_movementIntent->hasGoal();
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
        for (size_t i = 0; i < row->items.size(); ++i) {
            const tibia::container::TInventorySlot& slot = row->items[i];
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

void TGameSessionState::cancelAttackAndFollow()
{
    if (!canPerformGameAction())
        return;

    if (isFollowing())
        setFollowingCreatureId(0);

    if (isAttacking())
        setAttackingCreatureId(0);

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (storage->attackID())
            storage->setAttackID(0);
        if (storage->followID())
            storage->setFollowID(0);
    }

    tibia::game::localPlayerStopAutoWalk();

    gameclient::sendCancelAttackAndFollow();

    tibia::game::notify("onCancelAttackAndFollow");
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




void TGameSessionState::setChaseMode(const tibia::client::ChaseModes chaseMode)
{
    if (!canPerformGameAction())
        return;

    if (m_chaseMode == chaseMode)
        return;

    m_chaseMode = chaseMode;
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onChaseModeChange", chaseMode);
}

void TGameSessionState::setFightMode(const tibia::client::FightModes fightMode)
{
    if (!canPerformGameAction())
        return;

    if (m_fightMode == fightMode)
        return;

    m_fightMode = fightMode;
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onFightModeChange", fightMode);
}

void TGameSessionState::setSafeFight(const bool on)
{
    if (!canPerformGameAction())
        return;

    if (m_safeFight == on)
        return;

    m_safeFight = on;
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onSafeFightChange", on);
}

void TGameSessionState::setTactics(const tibia::client::ChaseModes chaseMode, const bool safeFight, tibia::client::PVPModes pvpMode)
{
    if (!canPerformGameAction())
        return;

    if (m_chaseMode == chaseMode && m_safeFight == safeFight && m_pvpMode == pvpMode)
        return;

    const bool chaseChanged = m_chaseMode != chaseMode;
    const bool safeChanged = m_safeFight != safeFight;
    const bool pvpChanged = m_pvpMode != pvpMode;
    m_chaseMode = chaseMode;
    m_safeFight = safeFight;
    m_pvpMode = pvpMode;
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    if (chaseChanged)
        tibia::game::notify("onChaseModeChange", chaseMode);
    if (safeChanged)
        tibia::game::notify("onSafeFightChange", safeFight);
    if (pvpChanged)
        tibia::game::notify("onPVPModeChange", pvpMode);
}

void TGameSessionState::setPVPMode(const tibia::client::PVPModes pvpMode)
{
    if (!canPerformGameAction())
        return;


    if (m_pvpMode == pvpMode)
        return;

    m_pvpMode = pvpMode;
    gameclient::sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onPVPModeChange", pvpMode);
}

void TGameSessionState::setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints)
{
    if (!canPerformGameAction())
        return;


    if (m_unjustifiedPoints == unjustifiedPoints)
        return;

    m_unjustifiedPoints = unjustifiedPoints;
    tibia::game::notify("onUnjustifiedPointsChange", unjustifiedPoints);
}

void TGameSessionState::setOpenPvpSituations(const uint8_t openPvpSituations)
{
    if (!canPerformGameAction())
        return;

    if (m_openPvpSituations == openPvpSituations)
        return;

    m_openPvpSituations = openPvpSituations;
    tibia::game::notify("onOpenPvpSituationsChange", openPvpSituations);
}





















void TGameSessionState::ping()
{
    if (!m_gameConnection || !m_gameConnection->isConnected())
        return;

    if (m_pingReceived != m_pingSent)
        return;

    gameclient::sendPing();
    ++m_pingSent;
    m_pingTimer.restart();
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
