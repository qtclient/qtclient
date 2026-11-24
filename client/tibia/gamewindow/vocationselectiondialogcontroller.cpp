#include "vocationselectiondialogcontroller.h"

#include "gamewindow/assignactiondialogs.h"

#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"

#include <protocol.pb.h>

#include <QQuickItem>
#include <stdexcept>

namespace tibia::gamewindow {

TVocationSelectionDialogController::TVocationSelectionDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                       QQuickItem* mapWindowPane, QObject* parent)
    : TDialogController(QStringLiteral("tutorial/VocationSelectionPopup.qml"), engine, dialogParentItem, parent)
    , m_mapWindowPane(mapWindowPane)
{
    // The popup starts on its first page; without a session it has no spell list model (the popup
    // finds no spell icons then).
    m_stage = 0;
}

void TVocationSelectionDialogController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    m_gameSession = std::move(GameSession);
    if (!m_gameSession)
        return;
    // Host: qtclient's proxy reads the spell infos itself (CipSoft hands it the session's).
    TSpellListDataModelSortFilterProxy* const spellList = new TSpellListDataModelSortFilterProxy(this);
    if (m_spellList)
        m_spellList->deleteLater();
    m_spellList = spellList;
    m_spellListModel = spellList;
    spellList->connectToGameSession(m_gameSession);
}

void TVocationSelectionDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    const auto* hint = input::gameActionAs<input::TGameActionShowTutorialHint>(pGameAction);
    if (!hint || hint->tutorialHintId() != 2)
        return;
    // Opens anew on the first page.
    dialog()->close();
    m_stage = 0;
    dialog()->create();
    dialog()->show();
}

void TVocationSelectionDialogController::requestClose() { dialog()->close(); }

QQuickItem* TVocationSelectionDialogController::mapWindowPane() { return m_mapWindowPane; }

void TVocationSelectionDialogController::selectVocation(const cip::tibia::qmlcomponents::enums::EVocation Vocation)
{
    // In a session the choice goes to the server: the protocol's vocation is
    // the same number (None..Monk convert one to one; any other throws).
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        const auto vocation = static_cast<uint32_t>(Vocation);
        if (vocation > 5)
            throw std::runtime_error("unhandled enum value");
        ::tibia::protobuf::protocol::GameclientMessageSetVocation message;
        message.set_vocation(vocation);
        Q_EMIT session->gameProtocolMessageHandler()->sendSetVocation(message);
    }
    dialog()->close();
}

void TVocationSelectionDialogController::nextPageClicked()
{
    // Stage 0 = Welcome to Newhaven; stage 1 = vocation cards (CIP QML).
    ++m_stage;
    Q_EMIT stageChanged();
}

}
