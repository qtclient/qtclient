#pragma once

// CIP-named reporting surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

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
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

class TRuleViolationProtocolMessageHandler final : public cip::tibia::reporting::TRuleViolationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::reporting::TRuleViolationProtocolMessageHandler::TRuleViolationProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

}
}
