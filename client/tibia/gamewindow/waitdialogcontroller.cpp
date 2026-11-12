#include "waitdialogcontroller.h"

#include <cmath>

namespace tibia::gamewindow {

TWaitDialogController::TWaitDialogController(QObject* parent)
    : TDialogController(QStringLiteral("WaitDialog.qml"), parent)
{
    m_fillPercentage = 0.0;
    m_tick.setInterval(200);
    connect(&m_tick, &QTimer::timeout, this, &TWaitDialogController::tick);
}

void TWaitDialogController::showCharacterList()
{
    present("loginwait_caption", "loginwait_text", Phase::CharacterList);
}

void TWaitDialogController::showGameWorld()
{
    present("connecting_caption", "connecting_description", Phase::GameWorld);
}

void TWaitDialogController::present(const char* captionId, const char* textId, const Phase phase)
{
    // close() destroys the QML item, so the next show() reads these again.
    m_phase = phase;
    m_dialogCaption = qtTrId(captionId);
    m_dialogText = qtTrId(textId);
    m_elapsed = 0;
    m_fillPercentage = 0.05;
    m_waitTextWithRemainingDuration = qtTrId("please_wait");
    Q_EMIT remainingWaitDurationChanged();
    if (!dialog()->isInStack())
        dialog()->show();
    m_tick.start();
}

void TWaitDialogController::close()
{
    m_tick.stop();
    m_phase = Phase::None;
    if (dialog()->isInStack())
        dialog()->close();
}

void TWaitDialogController::abortWait()
{
    m_abortedPhase = m_phase;
    m_tick.stop();
    m_phase = Phase::None;
    if (dialog()->isInStack())
        dialog()->close();
    Q_EMIT waitAborted();
    Q_EMIT aborted();
}

void TWaitDialogController::tick()
{
    // Indeterminate pulse while the login HTTP request is in flight.
    ++m_elapsed;
    m_fillPercentage = 0.05 + 0.9 * (0.5 + 0.5 * std::sin(m_elapsed * 0.35));
    Q_EMIT remainingWaitDurationChanged();
}

}
