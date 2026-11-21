#pragma once

// CIP-named reporting surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

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
namespace reporting {

/// CIP `reporting::TBugReportProtocolMessageHandler` — native Q→H for BugReportsAllowed.
class TBugReportProtocolMessageHandler final : public cip::tibia::reporting::TBugReportProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TBugReportProtocolMessageHandler(QObject* parent = nullptr);

    /// CIP slot: BugReportsAllowed (S 0x1A / 26) — native Q→H apply.
    void handleBugReportsAllowedMessage(
        const ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed& Message);

};

class TRuleViolationProtocolMessageHandler final : public cip::tibia::reporting::TRuleViolationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::reporting::TRuleViolationProtocolMessageHandler::TRuleViolationProtocolMessageHandler;
    /// The text the server sends for a report becomes the REPORT_TEXT action:
    /// its author, the text's edit id and the text itself.
    void handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message);
    void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message);
};

}
}
