#include "genericgameactionhandler.h"

#include "analysers/analyserstorages.h"
#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "gameactionhandlers.h"
#include "protocol/clientmessages.h"

#include <tibia/game/gamesession.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace tibia::input {

namespace {

using A = EGameAction;
using O = config::EClientOption;

/// A container slot's position: x 0xFFFF and the container id in y's low six
/// bits with 0x40 set.
bool isContainerPosition(const Position& position)
{
    return position.x == UINT16_MAX && (position.y & 0x40);
}

uint8_t containerIdOf(const Position& position)
{
    return static_cast<uint8_t>(position.y & 0x3F);
}

/// The object part of an object message: the action's type id and stack position.
bool hasObject(const TObjectGameAction& action)
{
    return action.objectClientId() != 0 && action.position().isValid() && action.stackPosition() >= 0;
}

/// Writes `text` to `filePath`, making its folder.
void saveTextToFile(const QString& filePath, const QString& text)
{
    if (filePath.isEmpty())
        return;
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(text.toUtf8());
}

void toggleBoolOption(config::TClientOptions* options, const O id)
{
    options->setBoolOption(id, !options->boolOption(id));
}

}

TGenericGameActionHandler::TGenericGameActionHandler(QObject* parent)
    : cip::tibia::input::TGenericGameActionHandler(parent)
{
    // The option toggles reach the options' listeners and the file through it.
    app::ClientOptionsNotifier::instance();
}

TGenericGameActionHandler::~TGenericGameActionHandler() = default;

void TGenericGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction)
        return;
    const IGameAction& action = *pGameAction;
    switch (action.gameActionType()) {
    case A::USE:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
        handleUseAction(static_cast<const TObjectGameAction&>(action));
        break;
    case A::LOOK:
        handleLookAction(static_cast<const TGameActionLook&>(action));
        break;
    case A::WRAP:
    case A::UNWRAP:
        handleWrapAndUnwrapAction(static_cast<const TObjectGameAction&>(action));
        break;
    case A::ROTATE_OBJECT:
        handleRotateAction(static_cast<const TGameActionRotate&>(action));
        break;
    case A::CONFIGURE_SHOW_OFF_SOCKET:
        handleConfigureShowOffSocketAction(static_cast<const TGameActionConfigureShowOffSocket&>(action));
        break;
    case A::GOD_TELEPORT:
        handleTeleportToCoordinateAction(static_cast<const TGameActionTeleportToCoordinate&>(action));
        break;
    case A::BROWSE_FIELD:
        handleBrowseFieldAction(static_cast<const TGameActionBrowseField&>(action));
        break;
    case A::MOVE_UP:
        handleMoveUpAction(static_cast<const TGameActionMoveUp&>(action));
        break;
    case A::QUIT_GAME:
        handleQuitGameAction();
        break;
    case A::TOGGLE_CREATURE_HUDS_VISIBLE:
        handleToggleCreatureHudsVisibleGameAction();
        break;
    case A::CHANGE_CHARACTER:
        handleChangeCharacterAction();
        break;
    case A::TOGGLE_FPS_LATENCY_INDICATOR:
        handleToggleFpsLagIndecatorGameAction();
        break;
    case A::TOGGLE_FULLSCREEN:
        handleToggleFullscreenAction();
        break;
    case A::SMART_CLICK:
        if (const IGameActionPtr& inner = static_cast<const TGameActionSmartClick&>(action).innerGameAction())
            Q_EMIT publishGameAction(inner);
        break;
    case A::REQUEST_RESOURCE_BALANCE:
        handleRequestResourceBalanceAction(static_cast<const TGameActionRequestResourceBalance&>(action));
        break;
    case A::RESET_HUNTING_SESSION:
        handleResetHuntingSessionAction();
        break;
    case A::SAVE_TEXT_TO_FILE:
        handleSaveTextToFile(static_cast<const TGameActionSaveTextToFile&>(action));
        break;
    case A::SAVE_JSON_TO_FILE:
        handleSaveJsonDocumentToFile(static_cast<const TGameActionSaveJsonDocumentToFile&>(action));
        break;
    case A::TOGGLE_SOUND:
        handleToggleSoundGameAction();
        break;
    default:
        break;
    }
}

void TGenericGameActionHandler::handleLookAction(const TGameActionLook& action)
{
    if (!action.position().isValid() || action.objectClientId() == 0 || action.stackPosition() < 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageLook message;
    tibia::protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                                  action.position(), action.stackPosition(), action.objectClientId());
    Q_EMIT sendLook(message);
}

void TGenericGameActionHandler::handleUseAction(const TObjectGameAction& action)
{
    if (!hasObject(action))
        return;
    uint8_t containerIndex = 0xFF;
    if (action.gameActionType() == A::OPEN) {
        containerIndex = isContainerPosition(action.position()) ? containerIdOf(action.position()) : 0;
    } else if (action.gameActionType() == A::OPEN_IN_NEW_WINDOW) {
        container::TContainerStorage* storage = container::TContainerStorage::instance();
        containerIndex = storage ? static_cast<uint8_t>(storage->findEmptyId()) : 0;
    }
    ::tibia::protobuf::protocol::GameclientMessageUseObject message;
    tibia::protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                                  action.position(), static_cast<uint8_t>(action.stackPosition()),
                                                  action.objectClientId());
    message.set_container_index(containerIndex);
    Q_EMIT sendUseObject(message);
}

void TGenericGameActionHandler::handleBrowseFieldAction(const TGameActionBrowseField& action)
{
    // Only a world map field can be browsed.
    if (!action.position().isValid() || action.position().x == UINT16_MAX)
        return;
    ::tibia::protobuf::protocol::GameclientMessageBrowseField message;
    message.mutable_position()->set_x(action.position().x);
    message.mutable_position()->set_y(action.position().y);
    message.mutable_position()->set_z(action.position().z);
    Q_EMIT sendBrowseField(message);
}

void TGenericGameActionHandler::handleMoveUpAction(const TGameActionMoveUp& action)
{
    if (!isContainerPosition(action.position()))
        return;
    Q_EMIT publishGameAction(std::make_shared<TGameActionMoveObject>(
        action.context(), Position(UINT16_MAX, static_cast<uint16_t>(0x40 | containerIdOf(action.position())), 0xFE)));
}

void TGenericGameActionHandler::handleWrapAndUnwrapAction(const TObjectGameAction& action)
{
    if (!action.position().isValid() || action.objectClientId() == 0 || action.stackPosition() < 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageToggleWrapState message;
    tibia::protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                                  action.position(), action.stackPosition(), action.objectClientId());
    Q_EMIT sendToggleWrapState(message);
}

void TGenericGameActionHandler::handleConfigureShowOffSocketAction(const TGameActionConfigureShowOffSocket& action)
{
    if (!hasObject(action))
        return;
    ::tibia::protobuf::protocol::GameclientMessageConfigureShowOffSocket message;
    tibia::protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                                  action.position(), static_cast<uint8_t>(action.stackPosition()),
                                                  action.objectClientId());
    Q_EMIT sendConfigureShowOffSocket(message);
}

void TGenericGameActionHandler::handleRotateAction(const TGameActionRotate& action)
{
    if (!action.position().isValid() || action.objectClientId() == 0 || action.stackPosition() < 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageTurnObject message;
    tibia::protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                                  action.position(), action.stackPosition(), action.objectClientId());
    Q_EMIT sendTurnObject(message);
}

void TGenericGameActionHandler::handleQuitGameAction()
{
    Q_EMIT sendQuitGame(::tibia::protobuf::protocol::GameclientMessageQuitGame());
}

void TGenericGameActionHandler::handleChangeCharacterAction()
{
    Q_EMIT showCharacterSelectionDialog();
}

void TGenericGameActionHandler::handleToggleFullscreenAction()
{
    config::TClientOptions* options = config::TClientOptions::instance();
    config::TClientWindowOptions window = options->clientWindowOptions();
    window.borderless = !window.borderless;
    options->setClientWindowOptions(window);
}

void TGenericGameActionHandler::handleRequestResourceBalanceAction(const TGameActionRequestResourceBalance& action)
{
    // The protocol's resource types are the client's; a type it does not know
    // is a programming error.
    static constexpr std::array<int, 26> ProtocolResourceTypes{ 0,  1,  2,  10, 20, 21, 30, 31, 32, 33, 50, 60, 70,
                                                                71, 72, 73, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89 };
    const int resourceType = static_cast<int>(action.resourceType());
    if (std::find(ProtocolResourceTypes.begin(), ProtocolResourceTypes.end(), resourceType) == ProtocolResourceTypes.end())
        throw std::runtime_error("unhandled enum value");
    ::tibia::protobuf::protocol::GameclientMessageRequestResourceBalance message;
    message.set_resource_type(static_cast<uint32_t>(resourceType));
    Q_EMIT sendRequestResourceBalance(message);
}

void TGenericGameActionHandler::handleTeleportToCoordinateAction(const TGameActionTeleportToCoordinate& action)
{
    ::tibia::protobuf::protocol::GameclientMessageTeleport message;
    message.mutable_position()->set_x(action.coordinate().x);
    message.mutable_position()->set_y(action.coordinate().y);
    message.mutable_position()->set_z(action.coordinate().z);
    Q_EMIT sendTeleport(message);
}

void TGenericGameActionHandler::handleResetHuntingSessionAction()
{
    if (analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance()) {
        storages->resetAll();
        storages->itemTracking()->clearRecentLootDrops();
    }
}

void TGenericGameActionHandler::handleToggleCreatureHudsVisibleGameAction()
{
    config::TClientOptions* options = config::TClientOptions::instance();
    const bool playerParts = options->boolOption(O::playerShowName) || options->boolOption(O::playerShowHealth)
                             || options->boolOption(O::playerShowMana) || options->boolOption(O::playerShowHarmonySerene)
                             || options->boolOption(O::playerShowMarks);
    const bool creatureParts = options->boolOption(O::creatureShowName) || options->boolOption(O::creatureShowHealth)
                               || options->boolOption(O::creatureShowMarks) || options->boolOption(O::creatueShowNpcIcon);
    if (playerParts && !creatureParts) {
        toggleBoolOption(options, O::playerHudEnabled);
        return;
    }
    if (creatureParts && !playerParts) {
        toggleBoolOption(options, O::creatureHudEnabled);
        return;
    }
    if (!playerParts && !creatureParts)
        return;
    // Both HUDs show something: player and creatures, creatures only, player
    // only, none, then both again.
    if (options->boolOption(O::playerHudEnabled)) {
        options->setBoolOption(O::playerHudEnabled, false);
        return;
    }
    const bool creatureHud = options->boolOption(O::creatureHudEnabled);
    options->setBoolOption(O::playerHudEnabled, true);
    options->setBoolOption(O::creatureHudEnabled, !creatureHud);
}

void TGenericGameActionHandler::handleSaveTextToFile(const TGameActionSaveTextToFile& action)
{
    saveTextToFile(action.filePath(), action.text());
}

void TGenericGameActionHandler::handleSaveJsonDocumentToFile(const TGameActionSaveJsonDocumentToFile& action)
{
    saveTextToFile(action.filePath(), QString::fromUtf8(action.document().toJson()));
}

void TGenericGameActionHandler::handleToggleSoundGameAction()
{
    config::TClientOptions* options = config::TClientOptions::instance();
    int volume = options->intOption(O::soundMasterVolumeOld);
    if (const int current = options->intOption(O::soundMasterVolume); current != 0) {
        options->setIntOption(O::soundMasterVolumeOld, current);
        volume = 0;
    }
    options->setIntOption(O::soundMasterVolume, volume);
}

void TGenericGameActionHandler::handleToggleFpsLagIndecatorGameAction()
{
    toggleBoolOption(config::TClientOptions::instance(), O::showFpsLagIndicator);
}

}
