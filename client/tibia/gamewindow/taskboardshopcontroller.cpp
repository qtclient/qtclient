#include "taskboardshopcontroller.h"
#include "colorhelper.h"

#include "taskboard/taskboardciptypes.h"

#include <client/const.h>
#include <client/outfit.h>

#include <QLocale>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

QColor outfitColor(const int index)
{
    const QColor c = tibia::shared::outfitColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

uint64_t huntingPoints()
{
    return tibia::game::hasLocalPlayer()
               ? tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_TASK_HUNTING)
               : 0;
}

}

TTaskboardShopController::TTaskboardShopController(QObject* parent)
    : cip::tibia::gamewindow::TTaskboardShopController(parent)
    , m_rewardsModel(new taskboard::TTaskboardRewardListModel(this))
{
    m_rewardList = m_rewardsModel;
    syncPlayerColors();
    refreshHuntingTaskPoints();
}

void TTaskboardShopController::showDialogTab()
{
    syncPlayerColors();
    refreshHuntingTaskPoints();
    taskboard::TTaskboardProtocolMessageHandler::requestShopRewards();
}

void TTaskboardShopController::hideDialogTab() {}

void TTaskboardShopController::updateRewardList()
{
    Q_EMIT rewardListChanged();
}

void TTaskboardShopController::requestBuyOffer(const int Index)
{
    const std::vector<taskboard::TShopReward>& rewards = m_rewardsModel->items();
    if (Index < 0 || Index >= static_cast<int>(rewards.size()))
        return;
    const taskboard::TShopReward& reward = rewards[static_cast<size_t>(Index)];
    if (reward.status != taskboard::EShopRewardStatus::Available)
        return;
    if (huntingPoints() < static_cast<uint64_t>(reward.price))
        return;
    taskboard::TTaskboardProtocolMessageHandler::buyShopReward(std::clamp(Index, 0, 0xffff));
}

void TTaskboardShopController::refreshHuntingTaskPoints()
{
    const int points = static_cast<int>(std::min<uint64_t>(huntingPoints(), INT_MAX));
    if (points == m_huntingTaskPoints)
        return;
    m_huntingTaskPoints = points;
    Q_EMIT huntingTaskPointsChanged();
}

void TTaskboardShopController::syncPlayerColors()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    const Outfit& outfit = tibia::game::localPlayerOutfit();
    m_headColor = outfitColor(outfit.getHead());
    m_torsoColor = outfitColor(outfit.getBody());
    m_legsColor = outfitColor(outfit.getLegs());
    m_detailColor = outfitColor(outfit.getFeet());
}

void TTaskboardShopController::setRewards(const std::vector<taskboard::TShopReward>& rewards)
{
    refreshHuntingTaskPoints();
    syncPlayerColors();
    m_rewardsModel->setItems(rewards);
    Q_EMIT rewardListChanged();
}

}
