#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

#include <vector>

namespace tibia {
namespace gamewindow {

/// BossDifficultySelectionDialog.qml (CIP: gamewindow::TBossDifficultySelectionDialogController,
/// ctor , protocol hookup). The server opens it before a lever boss
/// fight; changing the spinner previews the difficulty after 500 ms, Start / Cancel answer.
class TBossDifficultySelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TBossDifficultySelectionDialogController>
{
    Q_OBJECT
public:
    explicit TBossDifficultySelectionDialogController(QObject* parent = nullptr);

    /// CIP : restarts the preview timer when the value changes.
    void setSelectedDifficulty(int v) override;

public Q_SLOTS:
    /// CIP -> : action 1.
    void onCancelClicked() override;
    /// CIP -> : action 0 with the selected difficulty.
    void onStartClicked() override;

private:
    /// CIP : copies the selection data and shows the dialog. Crystal answers each
    /// preview with the full data, so an open dialog with the same constant properties is
    /// updated in place; otherwise it is rebuilt (QML reads constants once). The server
    /// closes the dialog after Start / Cancel.
    void open(const std::vector<std::any>& args);
    /// CIP : new modifiers; the selection follows the group leader's only when this
    /// player cannot change it.
    void update(const std::vector<std::any>& args);

    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP ctor: single shot, 500 ms; timeout -> (action 2).
    QTimer m_previewTimer;
};

}
}
