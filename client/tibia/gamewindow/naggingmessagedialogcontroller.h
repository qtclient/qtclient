#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QElapsedTimer>
#include <QTimer>

namespace tibia {
namespace gamewindow {

/// NaggingMessageDialog.qml (CIP: TNaggingMessageDialogController): a warning
/// with an optional countdown before OK unlocks (SHOW_NAGGING_DIALOG).
class TNaggingMessageDialogController final
    : public TDialogController<cip::tibia::gamewindow::TNaggingMessageDialogController>
{
    Q_OBJECT
public:
    explicit TNaggingMessageDialogController(QObject* parent = nullptr);
    ~TNaggingMessageDialogController() override;

    /// `text` is the body; `seconds` > 0 shows a countdown before OK enables.
    void showNagging(const QString& text, int seconds);

    Q_INVOKABLE void requestClose() override;

private:
    /// The whole seconds left of the countdown, announced when they change;
    /// the countdown stops at its end.
    void onRefreshSecondsRemaining();

    QTimer m_tick;
    QElapsedTimer m_clock;
    qint64 m_durationMs = 0;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
