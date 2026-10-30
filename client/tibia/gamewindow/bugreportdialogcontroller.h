#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <client/position.h>

namespace tibia::gamewindow {

/// The bug report, BugReportDialog.qml (CIP: TBugReportDialogController,
/// vtable 0x10262ddf0, game actions 0x100c4a040: REPORT_BUG_OTHER,
/// REPORT_BUG_MAP, REPORT_BUG_TYPO). A report goes out as the protocol's
/// BugReport; the server's BugReportsAllowed state blocks sending.
class TBugReportDialogController final : public TDialogController<cip::tibia::gamewindow::TBugReportDialogController>
{
    Q_OBJECT
public:
    /// CIP's bug categories, also the classification table DAT_10278fae0 (0x100c4ad10).
    enum ECategory : int
    {
        Map = 0,
        Typo = 1,
        Technical = 2,
        Other = 3,
    };

    explicit TBugReportDialogController(QObject* parent = nullptr);
    ~TBugReportDialogController() override;

    /// CIP 0x100c4a860: the row of the report's category.
    int preselectedClassificationIndex() const override;
    /// CIP 0x100c4a8b0.
    QStringList classifications() const override;
    /// CIP 0x100c4ab40: the reason the server refuses reports, empty if it takes them.
    QString notAllowedReason() const override;

public Q_SLOTS:
    /// CIP 0x100c4a2f0.
    void showReportOtherDialog() override;
    /// CIP 0x100c4a320.
    void showReportTypoDialog(const QString& SpeakerName, const QString& Text) override;
    /// CIP 0x100c4ac70: the dialog closes and the report resets to "other".
    void closeDialog() override;

public:
    /// CIP 0x100c4a410.
    Q_INVOKABLE void requestClose() override;
    /// CIP 0x100c4a420.
    Q_INVOKABLE void requestSendBugReport(int ClassificationIndex, const QString& Comment) override;

private:
    bool handleAction(const input::TGameAction& action);
    /// CIP 0x100c4a380: a map report about `position`.
    void showReportMapDialog(const Position& position);
    void show();
    /// CIP 0x100c4a600: the reported coordinate, else the player's position.
    Position reportedPosition() const;

    ECategory m_category = Other;
    Position m_position;
    QString m_speaker;
    QString m_text;
};

}
