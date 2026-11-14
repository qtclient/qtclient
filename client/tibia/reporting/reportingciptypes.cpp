#include "reporting/reportingciptypes.h"
#include <tibia/game/gamenotify.h>

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

void TRuleViolationProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/,
                                                                   const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow || opcode != 129 || !shadow->HasExtension(GameserverMessageTextForReport::gameserver_message_text_for_report))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageTextForReport::gameserver_message_text_for_report);
    tibia::game::notify("onTextForReport", m.statement_id(), m.text(), m.name());
}

} // namespace tibia::reporting
