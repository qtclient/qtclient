#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

namespace tibia {
namespace gamewindow {

/// CIP gamewindow::TWaitDialogController: WaitDialog.qml counting a fixed wait
/// down in 100 ms steps; at zero it closes and reports waitTimeElapsed, Abort
/// closes it and reports waitAborted.
class TWaitDialogController final : public TDialogController<cip::tibia::gamewindow::TWaitDialogController>
{
    Q_OBJECT
public:
    explicit TWaitDialogController(QObject* parent = nullptr);
    TWaitDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// Shows the dialog and starts a wait of `seconds`; `waitTextFormat`'s %1
    /// is the remaining time.
    void startWait(const QString& caption, const QString& text, qint64 seconds, const QString& waitTextFormat);
    /// Closes the dialog and stops the countdown.
    void close();

    /// The part of the wait already over, 0..1.
    double fillPercentage() const override;
    /// The wait text with the remaining time (seconds, then minutes).
    QString waitTextWithRemainingDuration() const override;

    Q_INVOKABLE void abortWait() override;

public Q_SLOTS:
    /// One timer step: counts the remaining time down by the timer interval,
    /// then restarts, or closes and reports waitTimeElapsed at zero.
    void reduceRemainingWaitDurationByOneSecond();

private:
    QString m_waitTextFormat;
    qint64 m_totalWaitDuration = 0;
    qint64 m_remainingWaitDuration = 0;
    QTimer m_timer;
};

/// A duration for display, rounded up: "N second(s)" up to 60 seconds, then
/// "N minutes".
QString formatRemainingTime(qint64 milliseconds);

}
}
