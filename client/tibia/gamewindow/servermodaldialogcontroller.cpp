#include "servermodaldialogcontroller.h"

#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"

#include <tibia/game/gamesession.h>
#include <protocol.pb.h>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

constexpr uint8_t NoSelection = 0xff;

int indexOf(const game::TServerModalDialogData::Options& options, const uint32_t id)
{
    const auto it = std::ranges::find_if(options, [id](const auto& option) { return static_cast<uint32_t>(option.first) == id; });
    return it == options.end() ? -1 : static_cast<int>(it - options.begin());
}

}

TServerModalDialogController::TServerModalDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("ServerModalDialog.qml"), Engine, DialogParentItem, parent)
{}

void TServerModalDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    // A priority dialog stays open when the session goes.
    if (GameSession || !m_dialogData.priority)
        dialog()->close();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->serverModalDialogProtocolMessageHandler().get(),
                                               &game::TServerModalDialogProtocolMessageHandler::openModalDialog, this,
                                               &TServerModalDialogController::showDialog));
}

void TServerModalDialogController::showDialog(const game::TServerModalDialogData& RequestedDialog)
{
    dialog()->close();
    m_dialogData = RequestedDialog;
    m_dialogTitle = m_dialogData.title;
    m_dialogText = m_dialogData.text;
    m_answerButtons.clear();
    for (const auto& [buttonId, label] : m_dialogData.buttons)
        m_answerButtons.append(label);
    m_selectionList.clear();
    for (const auto& [choiceId, label] : m_dialogData.choices)
        m_selectionList.append(label);
    m_defaultEnterButtonIndex = indexOf(m_dialogData.buttons, m_dialogData.defaultEnterButton);
    m_defaultEscapeButtonIndex = indexOf(m_dialogData.buttons, m_dialogData.defaultEscapeButton);
    dialog()->show();
}

/// CIP : out-of-range indices answer 0xff.
void TServerModalDialogController::answerButtonClicked(const int ButtonIndex, const int SelectionIndex)
{
    const auto idAt = [](const game::TServerModalDialogData::Options& options, const int index) {
        return index >= 0 && index < static_cast<int>(options.size()) ? static_cast<uint8_t>(options[index].first) : NoSelection;
    };
    const uint8_t choice = idAt(m_dialogData.choices, SelectionIndex);
    const uint8_t button = idAt(m_dialogData.buttons, ButtonIndex);
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        ::tibia::protobuf::protocol::GameclientMessageAnswerModalDialog message;
        message.set_dialog_id(m_dialogData.id);
        message.set_button(button);
        message.set_choice(choice);
        Q_EMIT session->serverModalDialogProtocolMessageHandler()->sendAnswerModalDialog(message);
    }
    dialog()->close();
}

}
