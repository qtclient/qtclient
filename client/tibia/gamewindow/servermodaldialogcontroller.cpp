#include "servermodaldialogcontroller.h"

#include <tibia/world/game.h>

namespace tibia::gamewindow {

namespace {

constexpr uint8_t NoSelection = 0xff;

int indexOf(const std::vector<uint8_t>& ids, const uint8_t id)
{
    const auto it = std::ranges::find(ids, id);
    return it == ids.end() ? -1 : static_cast<int>(it - ids.begin());
}

}

TServerModalDialogController::TServerModalDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ServerModalDialog.qml"), parent)
{}

void TServerModalDialogController::open(const uint32_t id, const QString& title, const QString& text, const Options& buttons,
                                        const uint8_t enterButton, const uint8_t escapeButton, const Options& choices)
{
    dialog()->close();
    m_id = id;
    m_dialogTitle = title;
    m_dialogText = text;
    m_buttonIds.clear();
    m_answerButtons.clear();
    for (const auto& [buttonId, label] : buttons) {
        m_buttonIds.push_back(buttonId);
        m_answerButtons.append(QString::fromStdString(label));
    }
    m_choiceIds.clear();
    m_selectionList.clear();
    for (const auto& [choiceId, label] : choices) {
        m_choiceIds.push_back(choiceId);
        m_selectionList.append(QString::fromStdString(label));
    }
    m_defaultEnterButtonIndex = indexOf(m_buttonIds, enterButton);
    m_defaultEscapeButtonIndex = indexOf(m_buttonIds, escapeButton);
    dialog()->show();
}

/// CIP 0x100c24af0: out-of-range indices answer 0xff.
void TServerModalDialogController::answerButtonClicked(const int ButtonIndex, const int SelectionIndex)
{
    const uint8_t button = ButtonIndex >= 0 && ButtonIndex < static_cast<int>(m_buttonIds.size()) ? m_buttonIds[ButtonIndex] : NoSelection;
    const uint8_t choice
        = SelectionIndex >= 0 && SelectionIndex < static_cast<int>(m_choiceIds.size()) ? m_choiceIds[SelectionIndex] : NoSelection;
    if (g_game.isOnline())
        g_game.answerModalDialog(m_id, button, choice);
    dialog()->close();
}

}
