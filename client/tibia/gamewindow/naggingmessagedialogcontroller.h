#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

namespace tibia::gamewindow {

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
    void onTick();

    QTimer m_tick;
};

}
