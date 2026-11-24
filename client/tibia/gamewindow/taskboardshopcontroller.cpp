#include "taskboardshopcontroller.h"
#include "colorhelper.h"

#include "appearances/appearanceinstance.h"
#include "creatures/creaturestorage.h"
#include "game/gamesessionbase.h"
#include "taskboard/taskboardciptypes.h"
#include "taskboard/taskboardcurrenciesstorage.h"
#include "taskboard/taskboardshopstorage.h"

#include <client/outfit.h>

namespace tibia::gamewindow {

TTaskboardShopController::TTaskboardShopController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : cip::tibia::gamewindow::TTaskboardShopController(parent)
    , m_rewardsModel(new taskboard::TTaskboardRewardListModel(this))
    , m_purchaseConfirmationDialog(engine, dialogParentItem)
{
    m_rewardList = m_rewardsModel;
    // CipSoft connects the confirmation on every session change; a repeated
    // confirmation finds no remembered offer, so one connection behaves alike.
    connect(&m_purchaseConfirmationDialog,
            &TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::confirmPurchase, this,
            &TTaskboardShopController::onPurchaseConfirmed);
}

void TTaskboardShopController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    hideDialogTab();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    if (taskboard::TTaskboardShopStorage* shopStorage = session->taskboardShopStorage().get())
        m_gameSessionConnections.add(connect(shopStorage, &taskboard::TTaskboardShopStorage::shopRewardDataChanged, this,
                                             &TTaskboardShopController::updateRewardList));
    if (taskboard::TTaskboardCurrenciesStorage* currencies = session->taskboardCurrenciesStorage().get())
        m_gameSessionConnections.add(connect(currencies,
                                             &taskboard::TTaskboardCurrenciesStorage::preyHuntingTaskTokensAmountChanged,
                                             this, &TTaskboardShopController::huntingTaskPointsChanged));
}

int TTaskboardShopController::huntingTaskPoints() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !session->taskboardCurrenciesStorage())
        return 0;
    return session->taskboardCurrenciesStorage()->preyHuntingTaskTokensAmount();
}

QObject* TTaskboardShopController::rewardList() const { return m_rewardsModel; }

QColor TTaskboardShopController::playerOutfitColor(const int part) const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return Qt::white;
    const std::shared_ptr<creatures::TCreature> player = session->playerCreature().lock();
    if (!player)
        return Qt::white;
    const auto outfit = std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(player->outfitAppearance());
    if (!outfit)
        return Qt::white;
    const Outfit& look = outfit->outfit();
    const int index = part == 0 ? look.getHead() : part == 1 ? look.getBody() : part == 2 ? look.getLegs() : look.getFeet();
    return tibia::shared::outfitColor(index);
}

QColor TTaskboardShopController::headColor() const { return playerOutfitColor(0); }

QColor TTaskboardShopController::torsoColor() const { return playerOutfitColor(1); }

QColor TTaskboardShopController::legsColor() const { return playerOutfitColor(2); }

QColor TTaskboardShopController::detailColor() const { return playerOutfitColor(3); }

void TTaskboardShopController::showDialogTab()
{
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !session->taskboardProtocolMessageHandler())
        return;
    session->taskboardProtocolMessageHandler()->requestShopRewards();
}

// The session's connections stay while the tab is hidden; only a new session
// drops them.
void TTaskboardShopController::hideDialogTab() { m_purchaseConfirmationDialog.dialog()->close(); }

void TTaskboardShopController::updateRewardList()
{
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_rewardsModel->setItems(session->taskboardShopStorage()->shopRewardData());
    Q_EMIT rewardListChanged();
}

void TTaskboardShopController::requestBuyOffer(const int Index)
{
    m_offerToBuyIndex = Index;
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    const std::vector<taskboard::TShopReward>& rewards = session->taskboardShopStorage()->shopRewardData();
    if (Index < 0 || Index >= static_cast<int>(rewards.size()))
        return;
    const taskboard::TShopReward& reward = rewards[static_cast<size_t>(Index)];
    if (reward.type == taskboard::EShopRewardType::Outfit)
        m_purchaseConfirmationDialog.showDialog(reward, headColor(), torsoColor(), legsColor(), detailColor());
    else
        m_purchaseConfirmationDialog.showDialog(reward);
}

void TTaskboardShopController::onPurchaseConfirmed()
{
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session || m_offerToBuyIndex < 0 || !session->taskboardProtocolMessageHandler())
        return;
    session->taskboardProtocolMessageHandler()->buyShopReward(m_offerToBuyIndex);
    m_offerToBuyIndex = -1;
}

}
