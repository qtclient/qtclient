#pragma once

#include "gamewindow/messagedialogcontroller.h"

#include <cip_gen.h>

#include <QVariant>

#include <memory>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace gamewindow {

// Tip MessageDialog (not cip::…::TMessageDialogController). Inside
// TDeathDialogController the CIP base injects that name and would shadow qtclient.
using TDeathMessageDialog = ::tibia::gamewindow::TMessageDialogController;

/// Stock "You are dead" dialog (CIP: TDeathDialogController, ctor —
/// MessageDialog with close-on-click + centered text). The session's game
/// protocol handler requests it (requestOpenDeathDialog); CIP's EDeathType and
/// TFairFightFactor are cipgen-skipped, so qtclient slots take ints. Live QML
/// path: composed TDeathMessageDialog → TibiaMessageDialog.qml.
class TDeathDialogController final : public cip::tibia::gamewindow::TDeathDialogController
{
    Q_OBJECT
public:
    TDeathDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TDeathDialogController() override;

    /// CIP: closes the dialog and follows the session: its death dialog request,
    /// OK (enter the world again), Cancel (log out), the store button and the
    /// player's world entered.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    /// CIP showDeadMessage — EDeathType / TFairFightFactor skipped in cipgen.
    void showDeadMessage(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    /// Keeps the request; the dialog opens on the fifth frame after it.
    void onRequestOpenDeathDialog(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    /// Cancel: QUIT_GAME, logging the character out.
    void onLogoutRequested() override;
    /// The store button enters the world again (accept) and opens the store
    /// (useful thing 11) once the character is back in the world.
    void onButtonClicked(
        cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;

    Q_INVOKABLE void onDialogButtonClicked(const QVariant& Identifier, bool CheckBoxIsChecked) override;
    Q_INVOKABLE void onDialogReturnKeyPressed() override;
    Q_INVOKABLE void onDialogEscapeKeyPressed() override;
    Q_INVOKABLE void onDialogKeyPressed(int Key, int Modifiers) override;

    /// A world was entered: the store opens if the store button was used.
    void onWorldEntered();
    /// Before each frame (the client's frame tick): counts down a requested
    /// death dialog and shows it when the count reaches 0.
    void onBeforeRenderFrame();

private:
    void closeBox();

    TDeathMessageDialog* m_box = nullptr;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    static constexpr int FramesBeforeDeadMessage = 5;
    /// -1: no death dialog requested.
    int m_framesUntilDeadMessage = -1;
    int m_requestedDeathType = 0;
    int m_requestedFairFightFactor = 0;
    bool m_requestedApplicableForSubsequentBlessing = false;
    bool m_finishing = false;
    bool m_openStoreOnWorldEntered = false;
};

}
}
