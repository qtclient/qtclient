#include "waitdialogcontroller.h"

#include <cmath>

namespace tibia::gamewindow {

TWaitDialogController::TWaitDialogController(QObject* parent)
    : TWaitDialogController(nullptr, nullptr, parent)
{
}

TWaitDialogController::TWaitDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("WaitDialog.qml"), engine, dialogParentItem, parent)
{
    m_timer.setInterval(100);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &TWaitDialogController::reduceRemainingWaitDurationByOneSecond);
}

void TWaitDialogController::startWait(const QString& caption, const QString& text, const qint64 seconds, const QString& waitTextFormat)
{
    m_dialogCaption = caption;
    m_dialogText = text;
    m_remainingWaitDuration = seconds * 1000;
    m_totalWaitDuration = seconds * 1000;
    m_waitTextFormat = waitTextFormat;
    dialog()->create();
    dialog()->show();
    m_timer.start();
}

void TWaitDialogController::close()
{
    dialog()->close();
    m_timer.stop();
}

double TWaitDialogController::fillPercentage() const
{
    if (m_totalWaitDuration == 0)
        return 0.0;
    return static_cast<double>(m_totalWaitDuration - m_remainingWaitDuration) / static_cast<double>(m_totalWaitDuration);
}

QString TWaitDialogController::waitTextWithRemainingDuration() const
{
    return m_waitTextFormat.arg(formatRemainingTime(m_remainingWaitDuration));
}

void TWaitDialogController::abortWait()
{
    m_timer.stop();
    close();
    Q_EMIT waitAborted();
}

void TWaitDialogController::reduceRemainingWaitDurationByOneSecond()
{
    if (m_remainingWaitDuration > 0)
        m_remainingWaitDuration -= m_timer.interval();
    if (m_remainingWaitDuration != 0) {
        m_timer.start();
        Q_EMIT remainingWaitDurationChanged();
        return;
    }
    close();
    Q_EMIT waitTimeElapsed();
}

QString formatRemainingTime(const qint64 milliseconds)
{
    const auto seconds = static_cast<uint>(static_cast<qint64>(std::ceil(static_cast<double>(milliseconds) / 1000.0)));
    if (seconds < 61)
        return qtTrId(seconds == 1 ? "time_second" : "time_seconds_plural").arg(seconds);
    return qtTrId("time_minutes").arg(static_cast<int>(std::ceil(seconds / 60.0)));
}

}
