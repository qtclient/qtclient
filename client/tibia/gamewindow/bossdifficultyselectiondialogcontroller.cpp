#include "bossdifficultyselectiondialogcontroller.h"

#include "game/gamesessionbase.h"
#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <string>

namespace tibia::gamewindow {

namespace {

enum EBossDifficultyAction : uint32_t { Start = 0, Cancel = 1, Preview = 2 };

template <class Strings>
QStringList toStringList(const Strings& strings)
{
    QStringList out;
    out.reserve(static_cast<qsizetype>(strings.size()));
    for (const std::string& s : strings)
        out.append(QString::fromStdString(s));
    return out;
}

}

TBossDifficultySelectionDialogController::TBossDifficultySelectionDialogController(QObject* parent)
    : TDialogController(QStringLiteral("BossDifficultySelectionDialog.qml"), parent)
{
    m_previewTimer.setInterval(500);
    m_previewTimer.setSingleShot(true);
    connect(&m_previewTimer, &QTimer::timeout, this, [this] { sendSelection(Preview, true); });
}

void TBossDifficultySelectionDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    dialog()->close();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    game::TBossDifficultySelectionProtocolMessageHandler* handler = session->bossDifficultySelectionProtocolMessageHandler();
    using Handler = game::TBossDifficultySelectionProtocolMessageHandler;
    using Self = TBossDifficultySelectionDialogController;
    m_gameSessionConnections.add(connect(handler, &Handler::openBossDifficultySelectionDialog, this, &Self::showDialog));
    m_gameSessionConnections.add(connect(handler, &Handler::updateBossDifficultySelectionDialog, this, &Self::updateDialog));
    m_gameSessionConnections.add(connect(handler, &Handler::closeBossDifficultySelectionDialog, this, [this] { dialog()->close(); }));
}

void TBossDifficultySelectionDialogController::showDialog(const game::TBossDifficultySelectionDialogData& data)
{
    m_selectedDifficulty = data.selectedDifficulty;
    m_updateData = data;
    m_conModifiers = toStringList(data.conModifiers);
    m_proModifiers = toStringList(data.proModifiers);
    m_isActiveUser = data.isActiveUser;
    m_context = data.context;
    m_raceId = data.raceId;
    m_lowestDifficulty = data.lowestDifficulty;
    m_highestDifficultyOfGroup = data.highestDifficultyOfGroup;
    m_personalHighestDifficulty = data.personalHighestDifficulty;
    m_currentBadLuckDropBonus = data.currentBadLuckDropBonus;
    m_playerNames = toStringList(data.playerNames);
    dialog()->create();
    dialog()->show();
}

void TBossDifficultySelectionDialogController::updateDialog(const game::TBossDifficultySelectionDialogUpdateData& data)
{
    m_updateData.selectedDifficulty = m_selectedDifficulty;
    if (m_updateData == data)
        return;
    m_updateData.proModifiers = data.proModifiers;
    m_updateData.conModifiers = data.conModifiers;
    m_proModifiers = toStringList(data.proModifiers);
    m_conModifiers = toStringList(data.conModifiers);
    if (!m_isActiveUser)
        m_selectedDifficulty = data.selectedDifficulty;
    Q_EMIT selectionDataUpdated();
}

void TBossDifficultySelectionDialogController::setSelectedDifficulty(const int v)
{
    if (m_selectedDifficulty == v)
        return;
    m_selectedDifficulty = v;
    m_previewTimer.start();
}

void TBossDifficultySelectionDialogController::onCancelClicked()
{
    sendSelection(Cancel, false);
}

void TBossDifficultySelectionDialogController::onStartClicked()
{
    sendSelection(Start, true);
}

void TBossDifficultySelectionDialogController::sendSelection(const uint32_t action, const bool withDifficulty) const
{
    // Host: the session refuses game actions while it cannot perform them.
    if (!tibia::game::canPerformGameAction())
        return;
    using namespace ::tibia::protobuf::protocol;
    auto message = tibia::protocol::newGameclientMessage(194);
    auto* m = message->MutableExtension(GameclientMessageBossDifficultySelection::gameclient_message_boss_difficulty_selection);
    m->set_context(m_context);
    m->set_action(action);
    if (withDifficulty)
        m->set_difficulty(static_cast<uint32_t>(m_selectedDifficulty));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

}
