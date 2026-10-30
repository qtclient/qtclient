#include "naggingmessagedialogcontroller.h"

#include "input/gameactionhandlers.h"

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TNaggingMessageDialogController::TNaggingMessageDialogController(QObject* parent)
    : TDialogController(QStringLiteral("NaggingMessageDialog.qml"), parent)
{
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &TNaggingMessageDialogController::onTick);
    input::setActionHandler(A::SHOW_NAGGING_DIALOG, [this](const input::TGameAction& action) {
        // CIP TGameActionShowNaggingDialog: text body, parameter = countdown
        // seconds (0 / omitted unlocks OK immediately).
        showNagging(action.text, action.parameter);
        return true;
    });
}

TNaggingMessageDialogController::~TNaggingMessageDialogController()
{
    input::setActionHandler(A::SHOW_NAGGING_DIALOG, nullptr);
    m_tick.stop();
}

void TNaggingMessageDialogController::showNagging(const QString& text, const int seconds)
{
    m_tick.stop();
    m_messageText = text;
    m_showCountdown = seconds > 0;
    m_secondsRemaining = m_showCountdown ? seconds : 0;
    Q_EMIT secondsRemainingChanged();
    dialog()->destroy();
    dialog()->create();
    dialog()->show();
    // QML enables OK only on secondsRemainingChanged → 0; force that path when
    // there is no countdown.
    if (!m_showCountdown) {
        m_secondsRemaining = 0;
        Q_EMIT secondsRemainingChanged();
    } else {
        m_tick.start();
    }
}

void TNaggingMessageDialogController::requestClose()
{
    m_tick.stop();
    dialog()->close();
}

void TNaggingMessageDialogController::onTick()
{
    if (m_secondsRemaining <= 0) {
        m_tick.stop();
        return;
    }
    --m_secondsRemaining;
    Q_EMIT secondsRemainingChanged();
    if (m_secondsRemaining <= 0)
        m_tick.stop();
}

}
