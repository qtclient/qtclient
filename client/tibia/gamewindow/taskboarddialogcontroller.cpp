#include "taskboarddialogcontroller.h"

#include "gamewindow/bountytaskscontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/taskboardshopcontroller.h"
#include "gamewindow/weeklytaskscontroller.h"
#include "input/gameactionhandlers.h"
#include "game/gamesessionbase.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardcurrenciesstorage.h"
#include "taskboard/taskboardshopstorage.h"
#include "taskboard/taskboardweeklytasksstorage.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <tibia/game/thingsession_forward.h>

#include <QLocale>

#include <algorithm>
#include <ctime>
#include <limits>

#include <any>
#include <map>
#include <string>
#include <vector>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using U32Map = std::map<std::string, uint32_t>;
using U32MapList = std::vector<U32Map>;
using StrMap = std::map<std::string, std::string>;
using StrMapList = std::vector<StrMap>;

QString formatBalance(const uint64_t value)
{
    return QLocale(QLocale::English).toString(static_cast<qulonglong>(value));
}

StrMapList shopOffers(const std::vector<taskboard::TShopRewardData>& rewards)
{
    using taskboard::EShopRewardStatus;
    using taskboard::EShopRewardType;
    StrMapList rows;
    uint32_t index = 0;
    for (const auto& reward : rewards) {
        TaskBoardShopItemData item;
        item.id = static_cast<uint8_t>(index++);
        item.offerType = static_cast<uint8_t>(reward.type);
        if (reward.type == EShopRewardType::PromotionPoint) {
            item.nextCost = reward.price;
            item.price = item.nextCost;
            item.currentPurchases = reward.purchaseCount > 0 ? reward.purchaseCount - 1 : 0;
            item.bought = (reward.status == EShopRewardStatus::Bought || item.nextCost == 0) ? 1 : 0;
        } else {
            item.title = reward.name.toStdString();
            item.description = reward.description.toStdString();
            item.price = reward.price;
            if (reward.type == EShopRewardType::TwoObjects)
                item.optionalItemId = reward.secondObjectTypeId;
            if (reward.type == EShopRewardType::Outfit) {
                item.lookType = reward.appearanceId;
                item.lookAddons = static_cast<uint8_t>((reward.firstAddon ? 1 : 0) | (reward.secondAddon ? 2 : 0));
            } else if (reward.type == EShopRewardType::Mount) {
                item.lookType = reward.appearanceId;
            } else {
                item.itemId = reward.appearanceId;
            }
            item.bought = reward.status == EShopRewardStatus::Bought ? 1 : 0;
        }
        rows.push_back({ { "id", std::to_string(item.id) },
                         { "offerType", std::to_string(item.offerType) },
                         { "title", item.title },
                         { "description", item.description },
                         { "price", std::to_string(item.price) },
                         { "bought", std::to_string(item.bought) },
                         { "lookType", std::to_string(item.lookType) },
                         { "lookAddons", std::to_string(item.lookAddons) },
                         { "itemId", std::to_string(item.itemId) },
                         { "optionalItemId", std::to_string(item.optionalItemId) },
                         { "maxPurchases", std::to_string(item.maxPurchases) },
                         { "currentPurchases", std::to_string(item.currentPurchases) },
                         { "nextCost", std::to_string(item.nextCost) } });
    }
    return rows;
}

}

std::vector<std::map<std::string, uint32_t>> taskboardBountyMonsterRows(const taskboard::TBountyTaskData& data)
{
    U32MapList rows;
    for (const auto& t : data.tasks) {
        const bool active = t.claimState == taskboard::EBountyTaskClaimState::Active
            || t.claimState == taskboard::EBountyTaskClaimState::Claimable;
        rows.push_back({ { "taskIndex", t.index },
                         { "raceId", t.raceId },
                         { "currentKills", t.currentKills },
                         { "totalKills", t.requiredKills },
                         { "rewardXp", t.rewardExperience },
                         { "rewardPoints", t.rewardBountyPoints },
                         { "rewardReroll", 1 },
                         { "rewardDust", t.rewardDust },
                         { "rarity", t.grade },
                         { "isActive", active ? 1u : 0u },
                         { "isCompleted", 0 },
                         { "isElite", t.elite ? 1u : 0u } });
    }
    return rows;
}

std::vector<std::map<std::string, uint32_t>> taskboardWeeklyMonsterRows(const taskboard::TWeeklyTaskData& w)
{
    U32MapList rows;
    const uint32_t anyTotal = w.anyCreatureRequiredKills;
    const uint32_t anyCurrent = w.anyCreatureCurrentKills;
    if (anyTotal > 0 || anyCurrent > 0)
        rows.push_back({ { "raceId", 0 }, { "current", anyCurrent }, { "total", anyTotal },
                         { "state", anyTotal > 0 && anyCurrent >= anyTotal ? 1u : 0u } });
    for (const auto& k : w.killTasks)
        rows.push_back({ { "raceId", k.raceId }, { "current", k.currentKills }, { "total", k.requiredKills },
                         { "state", k.requiredKills > 0 && k.currentKills >= k.requiredKills ? 1u : 0u } });
    return rows;
}

bool taskboardWeeklyActive(const taskboard::TWeeklyTaskData& w)
{
    return w.progressFinished == 0;
}

namespace {

}

TTaskboardDialogController::TTaskboardDialogController(QObject* parent)
    : TDialogController(QStringLiteral("taskboard/TaskboardDialog.qml"), parent)
    , m_bounty(new TBountyTasksController(this))
    , m_weekly(new TWeeklyTasksController(this))
    , m_shop(new TTaskboardShopController(this))
    , m_notOnMain(new TMessageDialogController(this))
{
    m_bountyTaskController = m_bounty;
    m_weeklyTaskController = m_weekly;
    connect(m_weekly, &TWeeklyTasksController::publishGameAction, this, &TTaskboardDialogController::publishGameAction);
    m_rewardShopController = m_shop;
    new TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(this);

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() { refreshFromResourceBalance(); });

    m_notOnMain->setCloseOnButtonClicked(true);
    // The action's parameter picks the tab: 0 bounty tasks, 1 weekly tasks.
    m_gameActions.on(A::OPEN_TASKBOARD_DIALOG, [this](const input::gameactions::TGameActionOpenTaskboardDialog& action) {
        if (action.tab() == 0)
            showDialog(BountyTasks);
        else if (action.tab() == 1)
            showDialog(WeeklyTasks);
        return true;
    });
}

TTaskboardDialogController::~TTaskboardDialogController()
{
}

void TTaskboardDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_notOnMain->dialog()->close();
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get())) {
        taskboard::TTaskboardCurrenciesStorage* currencies = session->taskboardCurrenciesStorage().get();
        using Currencies = taskboard::TTaskboardCurrenciesStorage;
        using Self = TTaskboardDialogController;
        m_gameSessionConnections.add(connect(currencies, &Currencies::bountyPointsChanged, this, &Self::bountyPointsChanged));
        m_gameSessionConnections.add(
            connect(currencies, &Currencies::preyHuntingTaskTokensAmountChanged, this, &Self::preyHuntingTaskTokensChanged));
        m_gameSessionConnections.add(connect(currencies, &Currencies::soulSealsAmountChanged, this, &Self::soulSealsChanged));
    }
    connectTabsToGameSession();
}

void TTaskboardDialogController::connectTabsToGameSession()
{
    m_bounty->connectToGameSession(m_gameSession);
    m_weekly->connectToGameSession(m_gameSession);
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    taskboard::TTaskboardShopStorage* shop = session->taskboardShopStorage().get();
    m_gameSessionConnections.add(connect(shop, &taskboard::TTaskboardShopStorage::shopRewardDataChanged, this,
                                         [this, shop]() { m_shop->applyShopData(shopOffers(shop->shopRewardData())); }));
}

QString TTaskboardDialogController::bountyPointsString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return formatBalance(session ? static_cast<uint64_t>(session->taskboardCurrenciesStorage()->bountyPoints()) : 0);
}

QString TTaskboardDialogController::preyHuntingTaskTokensString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return formatBalance(session ? static_cast<uint64_t>(session->taskboardCurrenciesStorage()->preyHuntingTaskTokensAmount()) : 0);
}

QString TTaskboardDialogController::soulSealsString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return formatBalance(session ? static_cast<uint64_t>(session->taskboardCurrenciesStorage()->soulSealsAmount()) : 0);
}

/// Reopens the dialog on `Tab`; before the character reached the main land
/// it only explains why there is no task board.
void TTaskboardDialogController::showDialog(const DialogTab& Tab)
{
    closeDialog();
    if (!tibia::game::hasLocalPlayer() || !tibia::game::localPlayerHasReachedMain()) {
        m_notOnMain->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                           QKeySequence(QStringLiteral("Return"))) });
        m_notOnMain->showMessage(qtTrId("taskboard_need_to_reach_main_caption"), qtTrId("taskboard_need_to_reach_main_text"));
        return;
    }
    m_dialogCreating = true;
    dialog()->show();
    m_dialogCreating = false;
    if (m_pendingResourceRefresh) {
        m_pendingResourceRefresh = false;
        refreshFromResourceBalance();
    }
    switchToTab(Tab);
}

void TTaskboardDialogController::closeDialog()
{
    m_bounty->clearNameFilter();
    dialog()->close();
}

/// While the bounty tab shows its preferred-monster list, closing only closes that list.
void TTaskboardDialogController::requestClose()
{
    if (m_dialogTab == BountyTasks && m_bounty->showPreferredList()) {
        m_bounty->setShowPreferredList(false);
        m_bounty->clearNameFilter();
        return;
    }
    closeDialog();
}

void TTaskboardDialogController::requestTabSwitch(const DialogTab& NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    switchToTab(NewTab);
}

/// Shows the new tab's page, then hides the previous one (also when it is the same page).
void TTaskboardDialogController::switchToTab(const DialogTab tab)
{
    const DialogTab previous = m_dialogTab;
    DialogTab shown = NoTabInitialState;
    switch (tab) {
    case BountyTasks:
        m_bounty->showDialogTab();
        shown = tab;
        break;
    case WeeklyTasks:
        m_weekly->showDialogTab();
        shown = tab;
        break;
    case Shop:
        m_shop->showDialogTab();
        shown = tab;
        break;
    case NoTabInitialState:
        break;
    }
    m_dialogTab = shown;
    Q_EMIT dialogTabChanged();
    switch (previous) {
    case BountyTasks:
        m_bounty->hideDialogTab();
        break;
    case WeeklyTasks:
        m_weekly->hideDialogTab();
        break;
    case Shop:
        m_shop->hideDialogTab();
        break;
    case NoTabInitialState:
        break;
    }
}

void TTaskboardDialogController::refreshFromResourceBalance()
{
    if (m_dialogCreating) {
        m_pendingResourceRefresh = true;
        return;
    }
    if (m_shop)
        m_shop->refreshHuntingTaskPoints();
}

}
