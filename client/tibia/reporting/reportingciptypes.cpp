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

void TBugReportProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{

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

namespace {

/// The text the server sends for a report becomes the REPORT_TEXT action:
/// its author, the text's edit id and the text itself.
void handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::REPORT_TEXT));
    action->text = QString::fromStdString(message.name());
    action->parameter = static_cast<int>(message.statement_id());
    action->statement = QString::fromStdString(message.text());
    input::publish(action);
}

/// The server's report message confirms a submitted rule violation report.
constexpr uint32_t MessageModeReport = 38;

void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message)
{
    if (message.mode() == MessageModeReport)
        tibia::game::notify("onRuleViolationReportConfirmed");
}

}

void TRuleViolationProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    if (opcode == 180 && shadow->HasExtension(GameserverMessageMessage::gameserver_message_message))
        handleMessageMessage(shadow->GetExtension(GameserverMessageMessage::gameserver_message_message));
    if (opcode == 129 && shadow->HasExtension(GameserverMessageTextForReport::gameserver_message_text_for_report))
        handleTextForReportMessage(shadow->GetExtension(GameserverMessageTextForReport::gameserver_message_text_for_report));
}

} // namespace tibia::reporting
