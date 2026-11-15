#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <client/position.h>

namespace tibia {
namespace gamewindow {

/// The bug report, BugReportDialog.qml, opened by the game actions
/// REPORT_BUG_OTHER, REPORT_BUG_MAP and REPORT_BUG_TYPO. A report goes out as the protocol's
/// BugReport; the server's BugReportsAllowed state blocks sending.
class TBugReportDialogController final : public TDialogController<cip::tibia::gamewindow::TBugReportDialogController>
{
    Q_OBJECT
public:
    /// The bug categories, also the order of the classification rows.
    enum ECategory : int
    {
        Map = 0,
        Typo = 1,
        Technical = 2,
        Other = 3,
    };

    explicit TBugReportDialogController(QObject* parent = nullptr);
    ~TBugReportDialogController() override;

    /// The row of the report's category.
    int preselectedClassificationIndex() const override;
    QStringList classifications() const override;
    /// The reason the server refuses reports, empty if it takes them.
    QString notAllowedReason() const override;

public Q_SLOTS:
    void showReportOtherDialog() override;
    void showReportTypoDialog(const QString& SpeakerName, const QString& Text) override;
    /// The dialog closes and the report resets to "other".
    void closeDialog() override;

public:
    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void requestSendBugReport(int ClassificationIndex, const QString& Comment) override;

private:
    bool handleAction(const input::TGameAction& action);
    /// A map report about `position`.
    void showReportMapDialog(const Position& position);
    void show();

    ECategory m_category = Other;
    Position m_position;
    QString m_speaker;
    QString m_text;
};

}
}
