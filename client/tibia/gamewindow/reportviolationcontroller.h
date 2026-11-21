#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QDateTime>

#include <memory>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace gamewindow {

/// The rule violation report, ReportViolationDialog.qml, opened by the game
/// actions REPORT_STATEMENT, REPORT_NAME, REPORT_BOT, REPORT_HIRELING_NAME,
/// REPORT_TEXT, GET_TEXT_FOR_REPORT). A report goes out as the protocol's
/// RuleViolationReport; the next one waits 10 minutes.
class TReportViolationController final : public TDialogController<cip::tibia::gamewindow::TReportViolationController>
{
    Q_OBJECT
public:
    /// The report types (typeName names them for dialogType).
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

    /// The reasons a report type offers.
    static std::vector<Reason> reasons(EType type);
    /// The dialogType of a report type.
    static QString typeName(EType type);
    void onRuleViolationReportConfirmedByServer();
    /// CIP connector: the dialog closes; the session's rule violation handler
    /// confirms the reports.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    bool handleAction(const input::IGameAction& action);
    void open(EType type, const QString& name, const QString& statement, uint32_t id);

    EType m_type = Name;
    uint32_t m_id = 0;
    int m_reasonId = -1;
    QDateTime m_lastReport;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;

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
