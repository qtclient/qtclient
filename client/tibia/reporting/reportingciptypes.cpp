#include "reporting/reportingciptypes.h"
#include <tibia/game/gamenotify.h>

#include "input/gameactionhandlers.h"
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
    // The four known states (0 = allowed, otherwise the refusal reason); an
    // unknown one rejects the message.
    const uint32_t state = Message.state();
    if (state > 3)
        return;
    tibia::game::setBugReportState(static_cast<int>(state));
    tibia::game::setCanReportBugs(state == 0);
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

namespace {

/// The server's report message confirms a submitted rule violation report.
constexpr uint32_t MessageModeReport = 38;

}

void TRuleViolationProtocolMessageHandler::handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::REPORT_TEXT));
    action->text = QString::fromStdString(message.name());
    action->parameter = static_cast<int>(message.statement_id());
    action->statement = QString::fromStdString(message.text());
    input::publish(action);
}

void TRuleViolationProtocolMessageHandler::handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message)
{
    if (message.mode() == MessageModeReport)
        tibia::game::notify("onRuleViolationReportConfirmed");
}

} // namespace tibia::reporting
