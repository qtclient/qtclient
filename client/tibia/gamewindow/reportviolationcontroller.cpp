#include "reportviolationcontroller.h"

#include "chat/chatchannel.h"
#include "input/gameactionhandlers.h"

#include <creatures/creaturestorage.h>
#include <tibia/game/gamesession.h>
#include "assets/thingtype.h"

#include <algorithm>
#include <tibia/game/gamenotify.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr A Handled[] = { A::REPORT_STATEMENT, A::REPORT_NAME, A::REPORT_BOT, A::REPORT_HIRELING_NAME, A::REPORT_TEXT, A::GET_TEXT_FOR_REPORT };

/// A report within ten minutes of the last one is refused.
constexpr qint64 ReportCooldownSeconds = 600;

QString nameOf(const input::TGameAction& action)
{
    if (!action.text.isEmpty() || !action.creatureId)
        return action.text;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(action.creatureId))
            return row->name();
    }
    return {};
}

}

TReportViolationController::TReportViolationController(QObject* parent)
    : TDialogController(QStringLiteral("ReportViolationDialog.qml"), parent)
{
    for (const A type : Handled)
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleAction(action); });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRuleViolationReportConfirmed", [this](GameObject*, const std::vector<std::any>&) {
        onRuleViolationReportConfirmedByServer();
        return std::any{};
    });
}

TReportViolationController::~TReportViolationController()
{
    for (const A type : Handled)
        input::setActionHandler(type, nullptr);
}

/// The report actions open the dialog for their report type;
/// GET_TEXT_FOR_REPORT asks the server for a text's author and content.
bool TReportViolationController::handleAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::REPORT_STATEMENT:
        open(Statement, nameOf(action), action.statement, static_cast<uint32_t>(action.parameter));
        return true;
    case A::REPORT_NAME:
        open(Name, nameOf(action), {}, 0);
        return true;
    case A::REPORT_BOT:
        open(Bot, nameOf(action), {}, 0);
        return true;
    case A::REPORT_HIRELING_NAME:
        open(Hireling, nameOf(action), {}, action.creatureId);
        return true;
    case A::REPORT_TEXT:
        open(Text, nameOf(action), action.statement, static_cast<uint32_t>(action.parameter));
        return true;
    case A::GET_TEXT_FOR_REPORT: {
        const uint16_t typeId = action.objectClientId;
        tibia::game::requestTextForReport(action.position, typeId, static_cast<uint8_t>(std::max(action.stackPosition, 0)));
        return true;
    }
    default:
        return false;
    }
}

void TReportViolationController::open(const EType type, const QString& name, const QString& statement, const uint32_t id)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (m_lastReport.isValid() && m_lastReport.secsTo(now) < ReportCooldownSeconds) {
        // A chat system message and the same line beneath the world map.
        const QString text = qtTrId("new_report_too_soon_error");
        for (const A messageType : { A::ADD_SYSTEM_CHAT_MESSAGE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE }) {
            auto message = input::mutableClone(*input::makeAction(messageType));
            message->text = text;
            message->color = chat::consoleColor(30);
            input::publish(message);
        }
        return;
    }
    dialog()->close();
    m_type = type;
    m_id = id;
    m_reasonId = 0;
    m_reportedCharacterName = name;
    m_reportedStatement = statement;
    m_selectedReportReasonName.clear();
    m_selectedReportReasonDescription.clear();
    m_dialogType = typeName(type);
    m_ruleViolationReasons.clear();
    for (const Reason& reason : reasons(type))
        m_ruleViolationReasons.append(qtTrId(reason.caption));
    dialog()->create();
    dialog()->show();
}

void TReportViolationController::selectReportReason(const int ModelIndex)
{
    const std::vector<Reason> list = reasons(m_type);
    if (ModelIndex < 0 || ModelIndex >= static_cast<int>(list.size()))
        return;
    const Reason& reason = list[static_cast<size_t>(ModelIndex)];
    m_selectedReportReasonName = qtTrId(reason.caption);
    m_selectedReportReasonDescription = qtTrId(reason.example);
    m_reasonId = reason.id;
    Q_EMIT selectedReportReasonChanged();
}

/// A bot report carries no translation.
void TReportViolationController::submitReport(const QString& Comment, const QString& Translation)
{
    const std::string translation = m_type == Bot ? std::string() : Translation.toStdString();
    tibia::game::reportRuleViolationCip(static_cast<uint8_t>(m_type), static_cast<uint8_t>(m_reasonId), m_reportedCharacterName.toStdString(),
                                  Comment.toStdString(), translation, m_id);
    m_lastReport = QDateTime::currentDateTime();
    dialog()->close();
}

void TReportViolationController::cancelReport()
{
    dialog()->close();
}

/// The server's confirmation lifts the ten-minute wait.
void TReportViolationController::onRuleViolationReportConfirmedByServer()
{
    m_lastReport = QDateTime();
}

QString TReportViolationController::typeName(const EType type)
{
    switch (type) {
    case Name:
        return QStringLiteral("name");
    case Statement:
        return QStringLiteral("statement");
    case Bot:
        return QStringLiteral("bot");
    case Hireling:
        return QStringLiteral("hireling");
    case Text:
        return QStringLiteral("text");
    }
    return {};
}

std::vector<TReportViolationController::Reason> TReportViolationController::reasons(const EType type)
{
    static const std::vector<Reason> NameReasons = {
        { "insulting_reason_caption", "insulting_name_example", 0 },
        { "racist_reason_caption", "racist_name_example", 0 },
        { "sexually_related_reason_caption", "sexually_related_name_example", 0 },
        { "drug_related_reason_caption", "drug_related_name_example", 0 },
        { "harassing_reason_caption", "harassing_name_example", 0 },
        { "generally_objectionable_reason_caption", "generally_objectionable_name_example", 0 },
        { "advertising_brand_reason_caption", "advertising_brand_name_example", 2 },
        { "advertising_rl_reason_caption", "advertising_rl_name_example", 2 },
        { "advertising_money_reason_caption", "advertising_money_name_example", 2 },
        { "religious_political_reason_caption", "religious_political_name_example", 3 },
        { "supporting_rule_violation_reason_caption", "supporting_rule_violation_name_example", 4 },
    };
    static const std::vector<Reason> StatementReasons = {
        { "insulting_reason_caption", "insulting_statement_example", 5 },
        { "racist_reason_caption", "racist_statement_example", 5 },
        { "sexually_related_reason_caption", "sexually_related_statement_example", 5 },
        { "drug_related_reason_caption", "drug_related_statement_example", 5 },
        { "harassing_reason_caption", "harassing_statement_example", 5 },
        { "generally_objectionable_reason_caption", "generally_objectionable_statement_example", 5 },
        { "duplicated_statement_reason_caption", "duplicated_statement_statement_example", 6 },
        { "badly_formatted_nonsensical_reason_caption", "badly_formatted_nonsensical_statement_example", 6 },
        { "advertising_brand_reason_caption", "advertising_brand_statement_example", 7 },
        { "advertising_rl_reason_caption", "advertising_rl_statement_example", 7 },
        { "advertising_money_reason_caption", "advertising_money_statement_example", 7 },
        { "religious_political_reason_caption", "religious_political_statement_example", 8 },
        { "off_topic_reason_caption", "off_topic_statement_example", 8 },
        { "violating_language_restriction_reason_caption", "violating_language_restriction_statement_example", 9 },
        { "disclosing_personal_data_reason_caption", "disclosing_personal_data_statement_example", 10 },
        { "supporting_rule_violation_reason_caption", "supporting_rule_violation_statement_example", 11 },
        { "bug_abuse_reason_caption", "bug_abuse_statement_example", 12 },
        { "using_unofficial_software_to_play_reason_caption", "using_unofficial_software_to_play_statement_example", 13 },
        { "pretending_to_be_cipsoft_reason_caption", "pretending_to_be_cipsoft_statement_example", 14 },
        { "wrong_information_about_cipsoft_reason_caption", "wrong_information_about_cipsoft_statement_example", 15 },
        { "boycott_cipsoft_reason_caption", "boycott_cipsoft_statement_example", 15 },
        { "false_information_to_cipsoft_reason_caption", "false_information_to_cipsoft_statement_example", 16 },
        { "account_theft_reason_caption", "account_theft_statement_example", 18 },
        { "attacking_cipsoft_reason_caption", "attacking_cipsoft_statement_example", 19 },
        { "applicable_law_reason_caption", "applicable_law_statement_example", 20 },
        { "tibia_service_agreement_reason_caption", "tibia_service_agreement_statement_example", 20 },
        { "right_third_party_reason_caption", "right_third_party_statement_example", 20 },
    };
    static const std::vector<Reason> TextReasons = {
        { "insulting_reason_caption", "insulting_statement_example", 5 },
        { "racist_reason_caption", "racist_statement_example", 5 },
        { "sexually_related_reason_caption", "sexually_related_statement_example", 5 },
        { "drug_related_reason_caption", "drug_related_statement_example", 5 },
        { "harassing_reason_caption", "harassing_statement_example", 5 },
        { "generally_objectionable_reason_caption", "generally_objectionable_statement_example", 5 },
        { "advertising_brand_reason_caption", "advertising_brand_statement_example", 7 },
        { "advertising_rl_reason_caption", "advertising_rl_statement_example", 7 },
        { "advertising_money_reason_caption", "advertising_money_statement_example", 7 },
        { "religious_political_reason_caption", "religious_political_statement_example", 8 },
        { "disclosing_personal_data_reason_caption", "disclosing_personal_data_statement_example", 10 },
        { "supporting_rule_violation_reason_caption", "supporting_rule_violation_statement_example", 11 },
        { "bug_abuse_reason_caption", "bug_abuse_statement_example", 12 },
        { "using_unofficial_software_to_play_reason_caption", "using_unofficial_software_to_play_statement_example", 13 },
        { "pretending_to_be_cipsoft_reason_caption", "pretending_to_be_cipsoft_statement_example", 14 },
        { "wrong_information_about_cipsoft_reason_caption", "wrong_information_about_cipsoft_statement_example", 15 },
        { "boycott_cipsoft_reason_caption", "boycott_cipsoft_statement_example", 15 },
        { "account_theft_reason_caption", "account_theft_statement_example", 18 },
        { "attacking_cipsoft_reason_caption", "attacking_cipsoft_statement_example", 19 },
        { "applicable_law_reason_caption", "applicable_law_statement_example", 20 },
        { "tibia_service_agreement_reason_caption", "tibia_service_agreement_statement_example", 20 },
        { "right_third_party_reason_caption", "right_third_party_statement_example", 20 },
    };
    static const std::vector<Reason> BotReasons = {
        { "using_unofficial_software_to_play_reason_caption", "using_unofficial_software_to_play_bot_example", 13 },
    };
    switch (type) {
    case Name:
    case Hireling:
        return NameReasons;
    case Statement:
        return StatementReasons;
    case Bot:
        return BotReasons;
    case Text:
        return TextReasons;
    }
    return {};
}

}
