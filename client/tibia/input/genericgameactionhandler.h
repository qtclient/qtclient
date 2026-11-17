#pragma once

#include "gameaction.h"

#include <cip_gen.h>

namespace tibia {
namespace input {

/// CIP input::TGenericGameActionHandler: the session's handler for the object
/// actions that become one client message (look, use, open, wrap, rotate,
/// show-off socket, browse field, teleport, resource balance), the actions
/// that become another action (smart click, move up), quit and change
/// character, the option toggles (fullscreen, creature HUDs, sound, FPS
/// indicator), the hunting-session reset and saving text or JSON to a file.
///
/// Host concern: CipSoft hands each client message to the session through a
/// send* signal carrying the protobuf message; qtclient's session send path is
/// the tibia::game functions, which the handlers call directly. The container
/// storage, the options and the analyser storages are qtclient's instances.
class TGenericGameActionHandler final : public cip::tibia::input::TGenericGameActionHandler
{
    Q_OBJECT
public:
    explicit TGenericGameActionHandler(QObject* parent = nullptr);
    ~TGenericGameActionHandler() override;
    TGenericGameActionHandler(const TGenericGameActionHandler&) = delete;
    TGenericGameActionHandler& operator=(const TGenericGameActionHandler&) = delete;

    void handleLookAction(const TGameActionLook& action);
    /// USE, OPEN and OPEN_IN_NEW_WINDOW: the use message with the container
    /// the server opens a container in: none (0xFF) for USE, the container
    /// the object lies in for OPEN (0 outside one), a free one for a new window.
    void handleUseAction(const TObjectGameAction& action);
    void handleBrowseFieldAction(const TGameActionBrowseField& action);
    /// MOVE_UP: moves the whole stack to the parent slot (254) of the
    /// container it lies in.
    void handleMoveUpAction(const TGameActionMoveUp& action);
    void handleWrapAndUnwrapAction(const TObjectGameAction& action);
    void handleConfigureShowOffSocketAction(const TGameActionConfigureShowOffSocket& action);
    void handleRotateAction(const TGameActionRotate& action);
    void handleQuitGameAction();
    void handleChangeCharacterAction();
    void handleRequestResourceBalanceAction(const TGameAction& action);
    void handleTeleportToCoordinateAction(const TGameAction& action);
    void handleSaveTextToFile(const TGameAction& action);
    void handleSaveJsonDocumentToFile(const TGameAction& action);

public Q_SLOTS:
    /// CIP v12: hands an action of a type it handles to its handler; other
    /// types are ignored. Connected to the router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
    /// Flips the client window's full-screen option (clientWindowIsBorderless).
    void handleToggleFullscreenAction() override;
    /// Resets the loot, waste, impact, damage input, hunting session, progress
    /// and item tracking analysers.
    void handleResetHuntingSessionAction() override;
    void handleToggleCreatureHudsVisibleGameAction() override;
    /// Mutes (keeping the volume in soundMasterVolumeOld) or restores the volume.
    void handleToggleSoundGameAction() override;
    void handleToggleFpsLagIndecatorGameAction() override;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);
    /// QUIT_GAME: the session logs the character out.
    void sendQuitGame();
};

}
}
