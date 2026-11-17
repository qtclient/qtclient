#include "bugreportdialogcontroller.h"

#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <iterator>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr A Handled[] = { A::REPORT_BUG_OTHER, A::REPORT_BUG_MAP, A::REPORT_BUG_TYPO };

/// The classification rows, in the dialog's order.
constexpr TBugReportDialogController::ECategory Classifications[] = {
    TBugReportDialogController::Map,
    TBugReportDialogController::Typo,
    TBugReportDialogController::Technical,
    TBugReportDialogController::Other,
};

}

TBugReportDialogController::TBugReportDialogController(QObject* parent)
    : TDialogController(QStringLiteral("BugReportDialog.qml"), parent)
{
    for (const A type : Handled)
        input::setActionHandler(type, [this](const input::IGameAction& action) { return handleAction(action); });
}

TBugReportDialogController::~TBugReportDialogController()
{
    for (const A type : Handled)
        input::setActionHandler(type, nullptr);
}

/// REPORT_BUG_OTHER keeps the category, REPORT_BUG_MAP and REPORT_BUG_TYPO
/// (speaker and text) set theirs.
bool TBugReportDialogController::handleAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::REPORT_BUG_OTHER:
        showReportOtherDialog();
        return true;
    case A::REPORT_BUG_MAP:
        if (const auto* bug = input::gameActionAs<input::TGameActionReportMapBug>(&action))
            showReportMapDialog(bug->coordinate());
        return true;
    case A::REPORT_BUG_TYPO:
        if (const auto* typo = input::gameActionAs<input::TGameActionReportTypo>(&action))
            showReportTypoDialog(typo->speaker(), typo->text());
        return true;
    default:
        return false;
    }
}

void TBugReportDialogController::showReportOtherDialog()
{
    closeDialog();
    show();
}

void TBugReportDialogController::showReportTypoDialog(const QString& SpeakerName, const QString& Text)
{
    closeDialog();
    m_category = Typo;
    m_speaker = SpeakerName;
    m_text = Text;
    show();
}

void TBugReportDialogController::showReportMapDialog(const Position& position)
{
    closeDialog();
    m_category = Map;
    m_position = position;
    show();
}

void TBugReportDialogController::show()
{
    dialog()->create();
    // The QML preselection handler first runs while the table has no rows yet.
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("preselectedClassificationIndex", -1);
        item->setProperty("preselectedClassificationIndex", preselectedClassificationIndex());
    }
    dialog()->show();
}

void TBugReportDialogController::closeDialog()
{
    dialog()->close();
    m_category = Other;
    m_position = Position();
    m_speaker.clear();
    m_text.clear();
}

void TBugReportDialogController::requestClose()
{
    closeDialog();
}

int TBugReportDialogController::preselectedClassificationIndex() const
{
    for (int row = 0; row < static_cast<int>(std::size(Classifications)); ++row) {
        if (Classifications[row] == m_category)
            return row;
    }
    return -1;
}

QStringList TBugReportDialogController::classifications() const
{
    QStringList list;
    for (const ECategory category : Classifications) {
        switch (category) {
        case Map: list.append(qtTrId("bugreport_type_map")); break;
        case Typo: list.append(qtTrId("bugreport_type_typo")); break;
        case Technical: list.append(qtTrId("bugreport_type_technical")); break;
        case Other: list.append(qtTrId("bugreport_type_other")); break;
        }
    }
    return list;
}

QString TBugReportDialogController::notAllowedReason() const
{
    if (!tibia::game::isOnline())
        return qtTrId("bugreport_not_allowed_reason_not_yet_set");
    switch (tibia::game::getBugReportState()) {
    case 0: return {};
    case 1: return qtTrId("bugreport_not_allowed_reason_blacklisted");
    case 2: return qtTrId("bugreport_not_allowed_reason_not_enough_loyalty");
    case 3: return qtTrId("bugreport_not_allowed_reason_too_many_reports");
    default: return qtTrId("bugreport_not_allowed_reason_not_yet_set");
    }
}

/// A row outside the table reports "other"; the dialog closes either way.
void TBugReportDialogController::requestSendBugReport(const int ClassificationIndex, const QString& Comment)
{
    if (tibia::game::isOnline()) {
        const ECategory category = ClassificationIndex >= 0 && ClassificationIndex < static_cast<int>(std::size(Classifications))
            ? Classifications[ClassificationIndex]
            : Other;
        const std::string comment = Comment.toStdString();
        switch (category) {
        case Map:
            tibia::game::reportBugCip(Map, comment, {}, {}, m_position);
            break;
        case Typo:
            tibia::game::reportBugCip(Typo, comment, m_speaker.toStdString(), m_text.toStdString(), {});
            break;
        default:
            tibia::game::reportBugCip(category, comment, {}, {}, {});
            break;
        }
    }
    closeDialog();
}

}
