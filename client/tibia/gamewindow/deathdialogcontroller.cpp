#include "deathdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

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
    m_box->setMessageDialogTextFormat(TDeathMessageDialog::STYLED_TEXT);
    connect(m_box, &TDeathMessageDialog::acceptClicked, this, &TDeathDialogController::onLogoutRequested);
    connect(m_box, &TDeathMessageDialog::buttonClicked, this, [this](Button id, bool) { onButtonClicked(id); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TDeathDialogController::onWorldEntered);

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::death, this,
            [this](int deathType, int penality) {
                onRequestOpenDeathDialog(deathType, penality, false);
            });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        closeBox();
    });
}

TDeathDialogController::~TDeathDialogController() = default;

/// The caption and, by death type, the text with the store button's hint; OK, Cancel and
/// the store button. An unfair death names the share of the penalty that applies.
void TDeathDialogController::showDeadMessage(const int deathType, const int fairFightFactor,
                                            const bool applicableForSubsequentBlessing)
{
    closeBox();
    tibia::game::setSceneFrozen(true);
    m_box->setButtons({
        new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))),
        new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Escape"))),
        new TMessageDialogButton(qtTrId("store"), Button::USER_DEFINED, QKeySequence()),
    });
    QString storeHint = qtTrId(deathType == 3 ? "death_dialog_store_button_no_loss" : "death_dialog_store_button");
    if (applicableForSubsequentBlessing)
        storeHint = qtTrId("death_dialog_store_button_subsequent_blessing");
    QString text;
    switch (deathType) {
    case 0:
        text = qtTrId("death_dialog_regular_death").arg(storeHint);
        break;
    case 1:
        text = qtTrId("death_dialog_unfair_death").arg(100 - (fairFightFactor & 0xff)).arg(storeHint);
        break;
    case 2:
        text = qtTrId("death_dialog_adventurers_blessing").arg(storeHint);
        break;
    case 3:
        text = qtTrId("death_dialog_no_penalty").arg(storeHint);
        break;
    default:
        return;
    }
    m_box->showMessage(qtTrId("death_dialog_caption"), text);
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
    // Host: the scene stays frozen while the death message shows.
    tibia::game::setSceneFrozen(false);
    auto quit = std::make_shared<input::TGameActionQuitGame>();
    quit->parameter = 1;
    input::publish(quit);
    m_finishing = false;
}

void TDeathDialogController::onButtonClicked(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    if (ButtonIdentifier != Button::USER_DEFINED)
        return;
    m_openStoreOnWorldEntered = true;
    // The death message has no check box.
    Q_EMIT m_box->acceptClicked(false);
}

void TDeathDialogController::onWorldEntered()
{
    if (!m_openStoreOnWorldEntered)
        return;
    m_openStoreOnWorldEntered = false;
    // The store opened at useful thing 11 (the death hint points there).
    auto open = std::make_shared<input::TGameActionOpenStore>();
    open->parameter = 3;
    open->instance = 11;
    input::publish(open);
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
