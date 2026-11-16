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

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP `reporting::TBugReportProtocolMessageHandler` — native Q→H for BugReportsAllowed.
class TBugReportProtocolMessageHandler final : public cip::tibia::reporting::TBugReportProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TBugReportProtocolMessageHandler(QObject* parent = nullptr);

    static TBugReportProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TBugReportProtocolMessageHandler* handler);

    /// CIP slot: BugReportsAllowed (S 0x1A / 26) — native Q→H apply.
    void handleBugReportsAllowedMessage(
        const ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed& Message);

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode);
};

class TRuleViolationProtocolMessageHandler final : public cip::tibia::reporting::TRuleViolationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::reporting::TRuleViolationProtocolMessageHandler::TRuleViolationProtocolMessageHandler;
    /// The text the server sends for a report becomes the REPORT_TEXT action:
    /// its author, the text's edit id and the text itself.
    static void handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message);
    static void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message);
};

}
}
