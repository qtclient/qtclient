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
    explicit TDeathDialogController(QObject* parent = nullptr);
    ~TDeathDialogController() override;

    /// CIP: closes the dialog and connects the session's death dialog request.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    /// CIP showDeadMessage — EDeathType / TFairFightFactor skipped in cipgen.
    void showDeadMessage(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    /// CIP onRequestOpenDeathDialog — forwards to showDeadMessage.
    void onRequestOpenDeathDialog(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    /// OK (and the store button): QUIT_GAME, logging the character out.
    void onLogoutRequested() override;
    /// The store button logs out too and opens the store (useful thing 11) when the
    /// next character enters the world.
    void onButtonClicked(
        cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;

    Q_INVOKABLE void onDialogButtonClicked(const QVariant& Identifier, bool CheckBoxIsChecked) override;
    Q_INVOKABLE void onDialogReturnKeyPressed() override;
    Q_INVOKABLE void onDialogEscapeKeyPressed() override;
    Q_INVOKABLE void onDialogKeyPressed(int Key, int Modifiers) override;

    /// A world was entered: the store opens if the store button was used.
    void onWorldEntered();

private:
    void closeBox();

    TDeathMessageDialog* m_box = nullptr;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    bool m_finishing = false;
    bool m_openStoreOnWorldEntered = false;
};

}
}
