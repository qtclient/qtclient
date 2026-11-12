#pragma once

#include "gamewindow/messagedialogcontroller.h"

#include <cip_gen.h>

#include <QVariant>

namespace tibia {
namespace gamewindow {

// Tip MessageDialog (not cip::…::TMessageDialogController). Inside
// TDeathDialogController the CIP base injects that name and would shadow qtclient.
using TDeathMessageDialog = ::tibia::gamewindow::TMessageDialogController;

/// Stock "You are dead" dialog (CIP: TDeathDialogController, ctor —
/// MessageDialog with close-on-click + centered text). OT servers deliver death
/// via opcode 0x28 → Game::processDeath → TGameNotify::death; CIP's
/// GameserverMessageDead / EDeathType + TFairFightFactor are cipgen-skipped, so
/// qtclient slots take ints. Live QML path: composed TDeathMessageDialog →
/// TibiaMessageDialog.qml (main.cpp keeps a stack instance).
class TDeathDialogController final : public cip::tibia::gamewindow::TDeathDialogController
{
    Q_OBJECT
public:
    explicit TDeathDialogController(QObject* parent = nullptr);
    ~TDeathDialogController() override;

public Q_SLOTS:
    /// CIP showDeadMessage — EDeathType / TFairFightFactor skipped in cipgen.
    void showDeadMessage(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    /// CIP onRequestOpenDeathDialog — forwards to showDeadMessage.
    void onRequestOpenDeathDialog(int deathType, int fairFightFactor, bool applicableForSubsequentBlessing);
    void onLogoutRequested() override;
    void onButtonClicked(
        cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;

    Q_INVOKABLE void onDialogButtonClicked(const QVariant& Identifier, bool CheckBoxIsChecked) override;
    Q_INVOKABLE void onDialogReturnKeyPressed() override;
    Q_INVOKABLE void onDialogEscapeKeyPressed() override;
    Q_INVOKABLE void onDialogKeyPressed(int Key, int Modifiers) override;

private:
    void closeBox();

    TDeathMessageDialog* m_box = nullptr;
    bool m_finishing = false;
};

}
}
