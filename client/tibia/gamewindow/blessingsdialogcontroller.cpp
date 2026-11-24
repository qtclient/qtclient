#include "blessingsdialogcontroller.h"
#include "game/currentgamesession.h"

#include "game/blessingsprotocolmessagehandler.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/qmlblessingdialogdata.h"
#include "input/gameactionhandlers.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP : the store offer of a blessing.
int blessingStoreOffer(const int id)
{
    switch (id) {
        case 0x2: return 12;
        case 0x4: return 4;
        case 0x8: return 5;
        case 0x10: return 6;
        case 0x20: return 7;
        case 0x40: return 8;
        case 0x80: return 9;
        case 0x100: return 10;
        default: return 0;
    }
}

}

TBlessingsDialogController::TBlessingsDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                       QObject* parent)
    : TDialogController(QStringLiteral("BlessingsDialog.qml"), engine, dialogParentItem, parent)
{
}

/// The dialog closes; the session's blessings handler opens it with its data.
void TBlessingsDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.add(connect(session->blessingsProtocolMessageHandler().get(),
                                         &game::TBlessingsProtocolMessageHandler::requestBlessingsDialog, this,
                                         &TBlessingsDialogController::showDialog));
}

/// The client's premium time: a change refreshes the shown data's premium
/// status.
void TBlessingsDialogController::connectToPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                   &TBlessingsDialogController::onPremiumStatusChanged);
    m_premiumTimeStorage = PremiumTimeStorage;
    if (m_premiumTimeStorage)
        connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                &TBlessingsDialogController::onPremiumStatusChanged);
}

void TBlessingsDialogController::showDialog(const game::TBlessingDialogData& BlessingDialogData)
{
    QObject* previous = m_blessingsData;
    m_blessingsData = new game::TQmlBlessingDialogData(game::TBlessingDialogData(BlessingDialogData),
                                                     m_premiumTimeStorage && m_premiumTimeStorage->isPremium());
    m_blessingsData->setParent(this);
    dialog()->destroy();
    if (previous)
        previous->deleteLater();
    dialog()->show();
}

void TBlessingsDialogController::closeDialog() { dialog()->close(); }

void TBlessingsDialogController::onPremiumStatusChanged()
{
    if (auto* data = qobject_cast<game::TQmlBlessingDialogData*>(m_blessingsData))
        data->setPremium(m_premiumTimeStorage && m_premiumTimeStorage->isPremium());
}

void TBlessingsDialogController::requestOpenStoreWithPremiumCategory()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0});
    Q_EMIT publishGameAction(action);
    requestClose();
}

void TBlessingsDialogController::requestOpenStoreWithBlessingOffer(const int BlessingId)
{
    const int offer = blessingStoreOffer(BlessingId);
    if (!offer)
        return;
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_USEFUL_THING, .usefulThing = offer});
    Q_EMIT publishGameAction(action);
    requestClose();
}

void TBlessingsDialogController::requestClose() { dialog()->close(); }

}
