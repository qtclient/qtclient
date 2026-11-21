#include "reporting/reportingciptypes.h"
#include <tibia/game/gamenotify.h>

#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

namespace tibia::reporting {

namespace {
using ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed;
} // namespace

TBugReportProtocolMessageHandler::TBugReportProtocolMessageHandler(QObject* parent)
    : cip::tibia::reporting::TBugReportProtocolMessageHandler(parent)
{
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

namespace {

/// The server's report message confirms a submitted rule violation report.
constexpr uint32_t MessageModeReport = 38;

}

void TRuleViolationProtocolMessageHandler::handleTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& message)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionReportText>(QString::fromStdString(message.name()),
                                                                 static_cast<int>(message.statement_id()),
                                                                 QString::fromStdString(message.text())));
}

void TRuleViolationProtocolMessageHandler::handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message)
{
    if (message.mode() == MessageModeReport)
        tibia::game::notify("onRuleViolationReportConfirmed");
}

} // namespace tibia::reporting
