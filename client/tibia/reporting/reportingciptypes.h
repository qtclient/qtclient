#pragma once

// CIP-named reporting surfaces (promoted from cipwire).

#include "reporting/reportingstorage.h"
#include "client/position.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <memory>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>

namespace tibia::protobuf::protocol {
class GameserverMessageMessage;
class GameserverMessageTextForReport;
}

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageBugReportsAllowed;
}
}
}

namespace tibia {
namespace protocol {
class TGameActionContextHelper;
}
namespace reporting {

/// CIP `reporting::TBugReportProtocolMessageHandler` — native Q→H for BugReportsAllowed.
class TBugReportProtocolMessageHandler final : public cip::tibia::reporting::TBugReportProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler writes the server's bug report state into the session's
    /// reporting storage.
    explicit TBugReportProtocolMessageHandler(std::shared_ptr<TReportingStorage> storage);

    /// CIP slot: BugReportsAllowed (S 0x1A / 26) — native Q→H apply.
    void handleBugReportsAllowedMessage(
        const ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed& Message);

Q_SIGNALS:
    void sendBugReport(const ::tibia::protobuf::protocol::GameclientMessageBugReport& BugReportMessage);

private:
    std::shared_ptr<TReportingStorage> m_storage;
};

class TRuleViolationProtocolMessageHandler final : public cip::tibia::reporting::TRuleViolationProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler keeps the session's game action context helper.
    explicit TRuleViolationProtocolMessageHandler(std::shared_ptr<protocol::TGameActionContextHelper> gameActionContextHelper);
    /// The text the server sends for a report becomes the REPORT_TEXT action:
    /// its author, the text's edit id and the text itself.
    void handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message);
    void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message);
    /// GameclientMessageRuleViolationReport: the report type (0 name, 1 statement, 2 bot, 3 hireling name,
    /// 4 text; any other throws std::runtime_error), the reason (0..20; any other throws
    /// std::invalid_argument), the reported name, the comment, the translation and the statement, hireling
    /// and text ids (0 where the type has none).
    void reportRuleViolation(uint32_t type, uint32_t reason, const QString& name, const QString& comment, const QString& translation,
                             uint32_t statementId, uint32_t hirelingId, uint32_t textId);
    void reportName(uint32_t reason, const QString& name, const QString& comment, const QString& translation);
    void reportStatement(uint32_t reason, const QString& name, const QString& comment, const QString& translation, uint32_t statementId);
    /// A bot report has no translation.
    void reportBot(uint32_t reason, const QString& name, const QString& comment);
    void reportHirelingName(uint32_t reason, const QString& name, const QString& comment, const QString& translation, uint32_t hirelingId);
    void reportText(uint32_t reason, const QString& name, const QString& comment, const QString& translation, uint32_t textId);
    /// GameclientMessageGetTextForReport for the object at the classic position.
    void requestTextForReport(const Position& position, uint16_t typeId, int stackPosition);

Q_SIGNALS:
    void sendRuleViolationReport(const ::tibia::protobuf::protocol::GameclientMessageRuleViolationReport& RuleViolationReportMessage);
    void sendGetTextForReport(const ::tibia::protobuf::protocol::GameclientMessageGetTextForReport& GetTextForReportMessage);

private:
    std::shared_ptr<protocol::TGameActionContextHelper> m_gameActionContextHelper;
};

}
}
