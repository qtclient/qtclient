#include "bugreportdialogcontroller.h"

#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <iterator>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr A Handled[] = { A::REPORT_BUG_OTHER, A::REPORT_BUG_MAP, A::REPORT_BUG_TYPO };

/// DAT_10278fae0 as fills it.
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
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleAction(action); });
}

TBugReportDialogController::~TBugReportDialogController()
{
    for (const A type : Handled)
        input::setActionHandler(type, nullptr);
}

/// CIP : REPORT_BUG_OTHER (0xac) keeps the category, REPORT_BUG_MAP
/// (0xad) and REPORT_BUG_TYPO (0xae, speaker and text) set theirs.
bool TBugReportDialogController::handleAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::REPORT_BUG_OTHER:
        showReportOtherDialog();
        return true;
    case A::REPORT_BUG_MAP:
        showReportMapDialog(action.position);
        return true;
    case A::REPORT_BUG_TYPO:
        showReportTypoDialog(action.text, action.statement);
        return true;
    default:
        return false;
    }
}

// b1/ba274560d523ed0a
void TBugReportDialogController::showReportOtherDialog()
{
    closeDialog();
    show();
}

// b1/c3deacb777fe8abb
void TBugReportDialogController::showReportTypoDialog(const QString& SpeakerName, const QString& Text)
{
    closeDialog();
    m_category = Typo;
    m_speaker = SpeakerName;
    m_text = Text;
    show();
}

// b1/a26601fe1b32ab1d
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

// b1/41e21d8683626d52
void TBugReportDialogController::closeDialog()
{
    dialog()->close();
    m_category = Other;
    m_position = Position();
    m_speaker.clear();
    m_text.clear();
}

// b1/c61b6e9585d892b1
void TBugReportDialogController::requestClose()
{
    closeDialog();
}

// b1/7e74e0b12b31f69d
int TBugReportDialogController::preselectedClassificationIndex() const
{
    for (int row = 0; row < static_cast<int>(std::size(Classifications)); ++row) {
        if (Classifications[row] == m_category)
            return row;
    }
    return -1;
}

// b1/6d7d1473d8f79b63
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

// b1/bade0b3208860910
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

// b1/f787f049dfd5540d
Position TBugReportDialogController::reportedPosition() const
{
    if (m_position.isValid())
        return m_position;
    return tibia::game::localPlayer() ? tibia::game::localPlayerPosition() : Position();
}

/// A row outside the table reports "other"; the dialog closes either way.
// b1/669b2063bf7c7eba
void TBugReportDialogController::requestSendBugReport(const int ClassificationIndex, const QString& Comment)
{
    if (tibia::game::isOnline()) {
        const ECategory category = ClassificationIndex >= 0 && ClassificationIndex < static_cast<int>(std::size(Classifications))
            ? Classifications[ClassificationIndex]
            : Other;
        const std::string comment = Comment.toStdString();
        switch (category) {
        case Map:
            tibia::game::reportBugCip(Map, comment, {}, {}, reportedPosition());
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
