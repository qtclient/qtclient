#include "reporting/reportingciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

namespace tibia::reporting {

namespace {
TBugReportProtocolMessageHandler* g_liveBugReportProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed;
} // namespace

TBugReportProtocolMessageHandler::TBugReportProtocolMessageHandler(QObject* parent)
    : cip::tibia::reporting::TBugReportProtocolMessageHandler(parent)
{
}

TBugReportProtocolMessageHandler* TBugReportProtocolMessageHandler::liveInstance()
{
    return g_liveBugReportProtocolHandler;
}

void TBugReportProtocolMessageHandler::setLiveInstance(TBugReportProtocolMessageHandler* handler)
{
    g_liveBugReportProtocolHandler = handler;
}

void TBugReportProtocolMessageHandler::handleBugReportsAllowedMessage(
    const GameserverMessageBugReportsAllowed& Message)
{
    // Same side effects as ProtocolGame::parseBugReport.
    // CIP BugReportsAllowed: 0 = allowed; any other value is a refusal reason.
    const auto state = static_cast<uint8_t>(Message.state());
    tibia::game::setBugReportState(state);
    tibia::game::setCanReportBugs(state == 0);
}

void TBugReportProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                                bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    switch (opcode) {
    case 26: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(
                GameserverMessageBugReportsAllowed::gameserver_message_bug_reports_allowed))
            return;
        handleBugReportsAllowedMessage(shadow->GetExtension(
            GameserverMessageBugReportsAllowed::gameserver_message_bug_reports_allowed));
        break;
    }
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    auto* handler = new TBugReportProtocolMessageHandler(parent);
    TBugReportProtocolMessageHandler::setLiveInstance(handler);
}

} // namespace tibia::reporting
