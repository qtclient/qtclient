#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

#include <vector>

namespace tibia::gamewindow {

/// BossDifficultySelectionDialog.qml (CIP: gamewindow::TBossDifficultySelectionDialogController,
/// ctor 0x100e488a0, protocol hookup 0x100e48ae0). The server opens it before a lever boss
/// fight; changing the spinner previews the difficulty after 500 ms, Start / Cancel answer.
class TBossDifficultySelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TBossDifficultySelectionDialogController>
{
    Q_OBJECT
public:
    explicit TBossDifficultySelectionDialogController(QObject* parent = nullptr);

    /// CIP 0x100e49000: restarts the preview timer when the value changes.
    void setSelectedDifficulty(int v) override;

public Q_SLOTS:
    /// CIP 0x100e49520 -> 0x100b52db0: action 1.
    void onCancelClicked() override;
    /// CIP 0x100e49550 -> 0x100b52d40: action 0 with the selected difficulty.
    void onStartClicked() override;

private:
    /// CIP 0x100e48d70: copies the selection data and shows the dialog. Crystal answers each
    /// preview with the full data, so an open dialog with the same constant properties is
    /// updated in place; otherwise it is rebuilt (QML reads constants once). The server
    /// closes the dialog after Start / Cancel.
    void open(const std::vector<std::any>& args);
    /// CIP 0x100e48da0: new modifiers; the selection follows the group leader's only when this
    /// player cannot change it.
    void update(const std::vector<std::any>& args);

    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP ctor: single shot, 500 ms; timeout 0x100e49b20 -> 0x100b52cd0 (action 2).
    QTimer m_previewTimer;
};

}
