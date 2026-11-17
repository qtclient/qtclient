#include "genericgameactionhandler.h"

#include "analysers/analyserstorages.h"
#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "cyclopedia/iteminfostorage.h"
#include "gameactionhandlers.h"
#include "protocol/clientmessages.h"

#include <tibia/game/gamesession.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>

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
    tibia::game::lookMapObject(action.position(), action.objectClientId(), action.stackPosition());
}

void TGenericGameActionHandler::handleUseAction(const TObjectGameAction& action)
{
    if (!tibia::game::canPerformGameAction() || !hasObject(action))
        return;
    uint8_t containerIndex = 0xFF;
    if (action.gameActionType() == A::OPEN) {
        containerIndex = isContainerPosition(action.position()) ? containerIdOf(action.position()) : 0;
    } else if (action.gameActionType() == A::OPEN_IN_NEW_WINDOW) {
        container::TContainerStorage* storage = container::TContainerStorage::instance();
        containerIndex = storage ? static_cast<uint8_t>(storage->findEmptyId()) : 0;
    }
    tibia::protocol::gameclient::sendUseItem(action.position(), action.objectClientId(),
                                             static_cast<uint8_t>(action.stackPosition()), containerIndex);
}

void TGenericGameActionHandler::handleBrowseFieldAction(const TGameActionBrowseField& action)
{
    // Only a world map field can be browsed.
    if (!action.position().isValid() || action.position().x == UINT16_MAX)
        return;
    tibia::game::browseField(action.position());
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
    tibia::game::wrapMapObject(action.position(), action.objectClientId(), action.stackPosition());
}

void TGenericGameActionHandler::handleConfigureShowOffSocketAction(const TGameActionConfigureShowOffSocket& action)
{
    if (!hasObject(action))
        return;
    tibia::game::configureShowOffSocket(action.position(), action.objectClientId(),
                                        static_cast<uint8_t>(action.stackPosition()));
}

void TGenericGameActionHandler::handleRotateAction(const TGameActionRotate& action)
{
    tibia::game::rotateMapObject(action.position(), action.objectClientId(), action.stackPosition());
}

void TGenericGameActionHandler::handleQuitGameAction()
{
    Q_EMIT sendQuitGame();
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
    tibia::game::requestResourceBalance(static_cast<uint8_t>(action.resourceType()));
}

void TGenericGameActionHandler::handleTeleportToCoordinateAction(const TGameActionTeleportToCoordinate& action)
{
    tibia::game::sendTeleport(action.coordinate());
}

void TGenericGameActionHandler::handleResetHuntingSessionAction()
{
    if (analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance())
        storages->resetAll();
    if (cyclopedia::TItemInfoStorage* itemTracking = cyclopedia::TItemInfoStorage::instance())
        itemTracking->clearRecentLootDrops();
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
