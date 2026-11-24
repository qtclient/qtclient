#include "reporting/reportingciptypes.h"
#include <tibia/game/gamenotify.h>

#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"
#include "protocol/clientmessages.h"

#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"

#include <protocol.pb.h>

#include <stdexcept>
#include <utility>

namespace tibia::reporting {

namespace {
using ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed;
} // namespace

TBugReportProtocolMessageHandler::TBugReportProtocolMessageHandler(std::shared_ptr<TReportingStorage> storage)
    : m_storage(std::move(storage))
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
    if (m_storage)
        m_storage->setBugReportState(static_cast<int>(state));
    // Host: the bug report dialog still reads the state from the session.
    if (tibia::game::TGameSessionBase* session = tibia::game::currentGameSession())
        session->setBugReportState(static_cast<int>(state));
}

namespace {

/// The server's report message confirms a submitted rule violation report.
constexpr uint32_t MessageModeReport = 38;

}

TRuleViolationProtocolMessageHandler::TRuleViolationProtocolMessageHandler(
    std::shared_ptr<protocol::TGameActionContextHelper> gameActionContextHelper)
    : m_gameActionContextHelper(std::move(gameActionContextHelper))
{
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
        Q_EMIT ruleViolationReportConfirmed();
}

void TRuleViolationProtocolMessageHandler::reportRuleViolation(const uint32_t type, const uint32_t reason, const QString& name,
                                                               const QString& comment, const QString& translation,
                                                               const uint32_t statementId, const uint32_t hirelingId,
                                                               const uint32_t textId)
{
    // The report types map one to one onto the protocol's (0..4).
    if (type > 4)
        throw std::runtime_error("unhandled enum value");
    if (reason > 20)
        throw std::invalid_argument("Invalid enum value");
    ::tibia::protobuf::protocol::GameclientMessageRuleViolationReport message;
    message.set_report_type(type);
    message.set_reason(reason);
    message.set_name(name.toStdString());
    message.set_comment(comment.toStdString());
    message.set_translation(translation.toStdString());
    message.set_statement_id(statementId);
    message.set_hireling_id(hirelingId);
    message.set_text_id(textId);
    Q_EMIT sendRuleViolationReport(message);
}

void TRuleViolationProtocolMessageHandler::reportName(const uint32_t reason, const QString& name, const QString& comment,
                                                      const QString& translation)
{
    reportRuleViolation(0, reason, name, comment, translation, 0, 0, 0);
}

void TRuleViolationProtocolMessageHandler::reportStatement(const uint32_t reason, const QString& name, const QString& comment,
                                                           const QString& translation, const uint32_t statementId)
{
    reportRuleViolation(1, reason, name, comment, translation, statementId, 0, 0);
}

void TRuleViolationProtocolMessageHandler::reportBot(const uint32_t reason, const QString& name, const QString& comment)
{
    reportRuleViolation(2, reason, name, comment, QString(), 0, 0, 0);
}

void TRuleViolationProtocolMessageHandler::reportHirelingName(const uint32_t reason, const QString& name, const QString& comment,
                                                              const QString& translation, const uint32_t hirelingId)
{
    reportRuleViolation(3, reason, name, comment, translation, 0, hirelingId, 0);
}

void TRuleViolationProtocolMessageHandler::reportText(const uint32_t reason, const QString& name, const QString& comment,
                                                      const QString& translation, const uint32_t textId)
{
    reportRuleViolation(4, reason, name, comment, translation, 0, 0, textId);
}

void TRuleViolationProtocolMessageHandler::requestTextForReport(const Position& position, const uint16_t typeId, const int stackPosition)
{
    ::tibia::protobuf::protocol::GameclientMessageGetTextForReport message;
    protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(), position,
                                           stackPosition, typeId);
    Q_EMIT sendGetTextForReport(message);
}

} // namespace tibia::reporting
