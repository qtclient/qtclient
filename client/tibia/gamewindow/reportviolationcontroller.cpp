#include "reportviolationcontroller.h"

#include "chat/chatchannel.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <tibia/world/map.h>
#include <tibia/world/thing.h>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr A Handled[] = { A::REPORT_STATEMENT, A::REPORT_NAME, A::REPORT_BOT, A::REPORT_HIRELING_NAME, A::REPORT_TEXT, A::GET_TEXT_FOR_REPORT };

/// CIP 0x100c2dfb0: a report within ten minutes of the last one is refused.
constexpr qint64 ReportCooldownSeconds = 600;

QString nameOf(const input::TGameAction& action)
{
    if (!action.text.isEmpty() || !action.creatureId)
        return action.text;
    const CreaturePtr creature = g_map.getCreatureById(action.creatureId);
    return creature ? QString::fromStdString(creature->getName()) : QString();
}

}

TReportViolationController::TReportViolationController(QObject* parent)
    : TDialogController(QStringLiteral("ReportViolationDialog.qml"), parent)
{
    for (const A type : Handled)
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleAction(action); });
    // CIP TRuleViolationProtocolMessageHandler::handleTextForReportMessage (0x100f87180): TextForReport (0x81) becomes
    // REPORT_TEXT with the name, the text id and the text (0x100e5a540).
    m_hooks.emplace_back("g_game", "onTextForReport", [](LuaObject*, const std::vector<std::any>& args) {
        auto action = input::mutableClone(*input::makeAction(A::REPORT_TEXT));
        action->parameter = static_cast<int>(client::gamecore::argInt(args, 0));
        action->statement = client::gamecore::argString(args, 1);
        action->text = client::gamecore::argString(args, 2);
        input::publish(action);
        return std::any{};
    });
}

TReportViolationController::~TReportViolationController()
{
    for (const A type : Handled)
        input::setActionHandler(type, nullptr);
}

/// CIP 0x100c2d720 and its per-type openers 0x100c2db90 (statement), 0x100c2da50
/// (name), 0x100c2daf0 (bot), 0x100c2dc50 (hireling), 0x100c2deb0 (text) and
/// 0x100c2dd40 (GET_TEXT_FOR_REPORT, the protocol's GetTextForReport).
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
        const uint16_t typeId = action.object && action.object->isItem() ? static_cast<uint16_t>(action.object->getId()) : 0;
        g_game.requestTextForReport(action.position, typeId, static_cast<uint8_t>(std::max(action.stackPosition, 0)));
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
        // CIP 0x100e5a6f0: ADD_SYSTEM_CHAT_MESSAGE in console colour 30, then the world map line.
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
    static const char* const TypeNames[] = { "name", "statement", "bot", "hireling", "text" };
    m_dialogType = QLatin1String(TypeNames[type]);
    m_ruleViolationReasons.clear();
    for (const Reason& reason : reasons(type))
        m_ruleViolationReasons.append(qtTrId(reason.caption));
    dialog()->create();
    dialog()->show();
}

/// CIP 0x100c2e670.
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

/// CIP 0x100c2e360.
void TReportViolationController::submitReport(const QString& Comment, const QString& Translation)
{
    g_game.reportRuleViolationCip(static_cast<uint8_t>(m_type), static_cast<uint8_t>(m_reasonId), m_reportedCharacterName.toStdString(),
                                  Comment.toStdString(), Translation.toStdString(), m_id);
    m_lastReport = QDateTime::currentDateTime();
    dialog()->close();
}

/// CIP 0x100c2e660.
void TReportViolationController::cancelReport()
{
    dialog()->close();
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
