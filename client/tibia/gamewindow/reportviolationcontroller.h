#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QDateTime>

#include <vector>

namespace tibia::gamewindow {

/// The rule violation report, ReportViolationDialog.qml (CIP:
/// TReportViolationController, vtable 0x102636c48, game actions 0x100c2d720:
/// REPORT_STATEMENT, REPORT_NAME, REPORT_BOT, REPORT_HIRELING_NAME,
/// REPORT_TEXT, GET_TEXT_FOR_REPORT). A report goes out as the protocol's
/// RuleViolationReport; the next one waits 10 minutes.
class TReportViolationController final : public TDialogController<cip::tibia::gamewindow::TReportViolationController>
{
    Q_OBJECT
public:
    /// CIP's report types (0x100c2d220 names them for dialogType).
    enum EType : int
    {
        Name = 0,
        Statement = 1,
        Bot = 2,
        Hireling = 3,
        Text = 4,
    };

    struct Reason
    {
        const char* caption;
        const char* example;
        int id;
    };

    explicit TReportViolationController(QObject* parent = nullptr);
    ~TReportViolationController() override;

    Q_INVOKABLE void submitReport(const QString& Comment, const QString& Translation) override;
    Q_INVOKABLE void cancelReport() override;
    Q_INVOKABLE void selectReportReason(int ModelIndex) override;

    /// CIP 0x100f80af0: the reasons a report type offers.
    static std::vector<Reason> reasons(EType type);

private:
    bool handleAction(const input::TGameAction& action);
    /// CIP 0x100c2dfb0.
    void open(EType type, const QString& name, const QString& statement, uint32_t id);

    EType m_type = Name;
    uint32_t m_id = 0;
    int m_reasonId = -1;
    QDateTime m_lastReport;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
