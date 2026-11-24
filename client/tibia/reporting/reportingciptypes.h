#pragma once

// CIP-named reporting surfaces (promoted from cipwire).

#include "reporting/reportingstorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <memory>

#include <QByteArray>
#include <QObject>

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

private:
    std::shared_ptr<protocol::TGameActionContextHelper> m_gameActionContextHelper;
};

}
}
