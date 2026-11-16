#include "naggingmessagedialogcontroller.h"

#include "input/gameactionhandlers.h"

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TNaggingMessageDialogController::TNaggingMessageDialogController(QObject* parent)
    : TDialogController(QStringLiteral("NaggingMessageDialog.qml"), parent)
{
    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, &TNaggingMessageDialogController::onRefreshSecondsRemaining);
    input::setActionHandler(A::SHOW_NAGGING_DIALOG, [this](const input::TGameAction& action) {
        // TGameActionShowNaggingDialog: the text and the countdown (seconds).
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
    m_messageText = text;
    m_showCountdown = seconds > 0;
    m_durationMs = std::max(0, seconds) * qint64(1000);
    m_clock.start();
    m_tick.start();
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

void TNaggingMessageDialogController::requestClose()
{
    m_tick.stop();
    dialog()->close();
}

void TNaggingMessageDialogController::onRefreshSecondsRemaining()
{
    const qint64 remaining = m_durationMs - m_clock.elapsed();
    if (remaining <= 0) {
        m_tick.stop();
        Q_EMIT secondsRemainingChanged();
        if (m_secondsRemaining == 0)
            return;
        m_secondsRemaining = 0;
        Q_EMIT secondsRemainingChanged();
        return;
    }
    const int seconds = static_cast<int>((remaining + 999) / 1000);
    if (seconds == m_secondsRemaining)
        return;
    m_secondsRemaining = seconds;
    Q_EMIT secondsRemainingChanged();
}

}
