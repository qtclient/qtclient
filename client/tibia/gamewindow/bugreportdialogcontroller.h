#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <client/position.h>

namespace tibia {
namespace gamewindow {

/// The bug report, BugReportDialog.qml (CIP: TBugReportDialogController,
/// vtable , game actions : REPORT_BUG_OTHER,
/// REPORT_BUG_MAP, REPORT_BUG_TYPO). A report goes out as the protocol's
/// BugReport; the server's BugReportsAllowed state blocks sending.
class TBugReportDialogController final : public TDialogController<cip::tibia::gamewindow::TBugReportDialogController>
{
    Q_OBJECT
public:
    /// CIP's bug categories, also the classification table DAT_10278fae0 .
    enum ECategory : int
    {
        Map = 0,
        Typo = 1,
        Technical = 2,
        Other = 3,
    };

    explicit TBugReportDialogController(QObject* parent = nullptr);
    ~TBugReportDialogController() override;

    /// CIP : the row of the report's category.
    int preselectedClassificationIndex() const override;
    /// CIP .
    QStringList classifications() const override;
    /// CIP : the reason the server refuses reports, empty if it takes them.
    QString notAllowedReason() const override;

public Q_SLOTS:
    /// CIP .
    void showReportOtherDialog() override;
    /// CIP .
    void showReportTypoDialog(const QString& SpeakerName, const QString& Text) override;
    /// CIP : the dialog closes and the report resets to "other".
    void closeDialog() override;

public:
    /// CIP .
    Q_INVOKABLE void requestClose() override;
    /// CIP .
    Q_INVOKABLE void requestSendBugReport(int ClassificationIndex, const QString& Comment) override;

private:
    bool handleAction(const input::TGameAction& action);
    /// CIP : a map report about `position`.
    void showReportMapDialog(const Position& position);
    void show();
    /// CIP : the reported coordinate, else the player's position.
    Position reportedPosition() const;

    ECategory m_category = Other;
    Position m_position;
    QString m_speaker;
    QString m_text;
};

}
}
