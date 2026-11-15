#pragma once

#include "client/gamecore.h"
#include "game/gameciptypes.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

#include <vector>

namespace tibia {
namespace gamewindow {

/// BossDifficultySelectionDialog.qml (CIP: gamewindow::TBossDifficultySelectionDialogController).
/// The server opens it before a lever boss fight; changing the spinner previews
/// the difficulty after 500 ms, Start / Cancel answer. The server closes it.
class TBossDifficultySelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TBossDifficultySelectionDialogController>
{
    Q_OBJECT
public:
    explicit TBossDifficultySelectionDialogController(QObject* parent = nullptr);

    /// Restarts the preview timer when the value changes (no notify: the
    /// spinner already shows it).
    void setSelectedDifficulty(int v) override;

    /// Takes the open message's data and shows the dialog.
    void showDialog(const game::TBossDifficultySelectionDialogData& data);

public Q_SLOTS:
    /// Answers "cancel" with the selection context.
    void onCancelClicked() override;
    /// Answers "start" with the selection context and the selected difficulty.
    void onStartClicked() override;

private:
    /// Takes new modifiers when they differ; the selection follows the server's
    /// only when this player cannot change it.
    void updateDialog(const game::TBossDifficultySelectionDialogUpdateData& data);
    /// Sends the boss difficulty selection client message.
    void sendSelection(uint32_t action, bool withDifficulty) const;

    std::vector<client::gamecore::Hook> m_hooks;
    /// The last update part, compared against the next update.
    game::TBossDifficultySelectionDialogUpdateData m_updateData;
    uint32_t m_context = 0;
    /// Single shot, 500 ms: sends a preview of the selected difficulty.
    QTimer m_previewTimer;
};

}
}
