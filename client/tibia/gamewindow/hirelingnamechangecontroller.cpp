#include "hirelingnamechangecontroller.h"

#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/genericentertextdialogcontroller.h"

namespace tibia::gamewindow {

THirelingNameChangeController::THirelingNameChangeController(QObject* parent)
    : cip::tibia::gamewindow::THirelingNameChangeController(parent)
    , m_enterTextDialog(new TGenericEnterTextDialogController(qtTrId("request_hireling_name_change_caption"),
                                                              qtTrId("request_hireling_name_change_text"), 30, this))
{
    connect(m_enterTextDialog, &TGenericEnterTextDialogController::textEntered, this, &THirelingNameChangeController::onTextEntered);
}

THirelingNameChangeController::~THirelingNameChangeController()
{
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
}

void THirelingNameChangeController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_enterTextDialog->dialog()->close();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !session->playerProtocolMessageHandler())
        return;
    m_gameSessionConnections.push_back(connect(session->playerProtocolMessageHandler().get(),
                                               &game::TPlayerProtocolMessageHandler::showHirelingNameChangeConfiguration, this,
                                               &THirelingNameChangeController::onShowHirelingNameChangeConfiguration));
}

void THirelingNameChangeController::onShowHirelingNameChangeConfiguration(const quint32 CreatureID, const quint32 ObjectID)
{
    m_creatureId = CreatureID;
    m_objectId = ObjectID;
    m_enterTextDialog->showWithText(QString());
}

void THirelingNameChangeController::onTextEntered()
{
    if (m_enterTextDialog->enteredText().isEmpty())
        return;
    m_name = m_enterTextDialog->enteredText();
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetHirelingName>(m_name, static_cast<int>(m_objectId),
                                                                               static_cast<int>(m_creatureId)));
}

}
