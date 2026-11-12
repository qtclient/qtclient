#include "deathdialogcontroller.h"

#include <QKeySequence>

#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Button = TDeathMessageDialog::Button;

}

TDeathDialogController::TDeathDialogController(QObject* parent)
    : cip::tibia::gamewindow::TDeathDialogController(parent)
    , m_box(new TDeathMessageDialog(this))
{
    // CIP ctor : setCloseOnButtonClicked(1), setTextCentered(1).
    m_box->setCloseOnButtonClicked(true);
    m_box->setTextCentered(true);
    m_box->setButtons({
        new TMessageDialogButton(QStringLiteral("Ok"), Button::OK, QKeySequence(QStringLiteral("Return"))),
    });
    // CIP routes MessageDialog buttonClicked → onButtonClicked → onLogoutRequested
    // (OK/YES). Do not also hook acceptClicked or logout runs twice.
    connect(m_box, &TDeathMessageDialog::buttonClicked, this,
            [this](Button id, bool) { onButtonClicked(id); });

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::death, this,
            [this](int deathType, int penality) {
                onRequestOpenDeathDialog(deathType, penality, false);
            });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        closeBox();
    });
}

TDeathDialogController::~TDeathDialogController() = default;

void TDeathDialogController::showDeadMessage(const int deathType, const int fairFightFactor,
                                            const bool applicableForSubsequentBlessing)
{
    Q_UNUSED(deathType);
    Q_UNUSED(applicableForSubsequentBlessing);
    closeBox();
    tibia::game::setSceneFrozen(true);

    QString message = QStringLiteral("You are dead.");
    // OT death packet omits penality when the feature is off; argInt then yields 0.
    // CIP FairFightFactor maps to the same percent when present.
    if (fairFightFactor > 0 && fairFightFactor < 100)
        message += QStringLiteral("\n\nFair fight reduction: %1%.").arg(fairFightFactor);
    message += QStringLiteral("\n\nClick Ok to leave this character.");

    m_box->showMessage(QStringLiteral("You are dead"), message);
}

void TDeathDialogController::onRequestOpenDeathDialog(const int deathType, const int fairFightFactor,
                                                     const bool applicableForSubsequentBlessing)
{
    showDeadMessage(deathType, fairFightFactor, applicableForSubsequentBlessing);
}

void TDeathDialogController::onLogoutRequested()
{
    if (m_finishing)
        return;
    m_finishing = true;
    closeBox();
    tibia::game::setSceneFrozen(false);
    if (tibia::game::isOnline())
        tibia::game::safeLogout();
    m_finishing = false;
}

void TDeathDialogController::onButtonClicked(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    switch (ButtonIdentifier) {
    case Button::OK:
    case Button::YES:
        onLogoutRequested();
        break;
    case Button::CANCEL:
    case Button::NO:
        closeBox();
        break;
    default:
        break;
    }
}

void TDeathDialogController::onDialogButtonClicked(const QVariant& Identifier, const bool CheckBoxIsChecked)
{
    m_box->onDialogButtonClicked(Identifier, CheckBoxIsChecked);
}

void TDeathDialogController::onDialogReturnKeyPressed()
{
    m_box->onDialogReturnKeyPressed();
}

void TDeathDialogController::onDialogEscapeKeyPressed()
{
    m_box->onDialogEscapeKeyPressed();
}

void TDeathDialogController::onDialogKeyPressed(const int Key, const int Modifiers)
{
    m_box->onDialogKeyPressed(Key, Modifiers);
}

void TDeathDialogController::closeBox()
{
    if (m_box && m_box->dialog()->isInStack())
        m_box->dialog()->close();
}

}
