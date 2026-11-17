#include "taskboarddialogcontroller.h"

#include "gamewindow/bountytaskscontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/taskboardshopcontroller.h"
#include "gamewindow/weeklytaskscontroller.h"
#include "input/gameactionhandlers.h"
#include "taskboard/taskboardbountytasksstorage.h"
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
#include <tibia/game/gamenotify.h>
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

uint64_t resourceBalance(const tibia::client::ResourceTypes_t type)
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerResourceBalance(type) : 0;
}

QString formatBalance(const uint64_t value)
{
    return QLocale(QLocale::English).toString(static_cast<qulonglong>(value));
}



constexpr uint8_t TASK_BOARD_WEEKLY_BASE_SLOTS = 6;
constexpr uint8_t TASK_BOARD_WEEKLY_EXPANDED_SLOTS = 9;
constexpr uint32_t TASK_BOARD_SECONDS_PER_DAY = 24 * 60 * 60;

uint8_t talismanMaxLevel(const uint8_t pathIndex)
{
    return pathIndex == 3 ? 180 : 166;
}

uint16_t talismanBonusHundredths(const uint8_t level, const uint8_t pathIndex)
{
    if (level == 0)
        return 0;
    switch (pathIndex) {
    case 0:
    case 1:
    case 2:
        if (level <= 26)
            return static_cast<uint16_t>(250 + (static_cast<uint32_t>(level) - 1U) * 50U);
        return static_cast<uint16_t>(std::min<uint32_t>(1500U + (static_cast<uint32_t>(level) - 26U) * 25U, 5000U));
    case 3:
        if (level <= 20)
            return static_cast<uint16_t>(static_cast<uint32_t>(level) * 100U);
        return static_cast<uint16_t>(std::min<uint32_t>(2000U + (static_cast<uint32_t>(level) - 20U) * 50U, 10000U));
    default:
        return 0;
    }
}

uint8_t remainingDaysUntil(const uint32_t unixTimestamp)
{
    if (unixTimestamp == 0)
        return 0;
    const auto now = static_cast<uint32_t>(std::time(nullptr));
    if (unixTimestamp <= now)
        return 0;
    const auto rawDays = (unixTimestamp - now + TASK_BOARD_SECONDS_PER_DAY - 1) / TASK_BOARD_SECONDS_PER_DAY;
    return static_cast<uint8_t>(std::min<uint32_t>(rawDays, 255));
}

std::vector<uint16_t> allMonsterRaceIds()
{
    const auto& races = tibia::game::getAllRaces();
    std::vector<uint16_t> raceIds;
    raceIds.reserve(races.size());
    for (const auto& race : races) {
        if (race.boss || race.raceId == 0 || race.raceId > std::numeric_limits<uint16_t>::max())
            continue;
        raceIds.emplace_back(static_cast<uint16_t>(race.raceId));
    }
    std::sort(raceIds.begin(), raceIds.end());
    raceIds.erase(std::unique(raceIds.begin(), raceIds.end()), raceIds.end());
    return raceIds;
}

U32Map bountyHeader(const taskboard::TBountyTaskData& data)
{
    return {
        { "rerollPoints", static_cast<uint32_t>(tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS)) },
        { "claimDaily", 0 },
        { "difficulty", static_cast<uint32_t>(data.difficulty) + 1 },
        { "eliteEnabled", data.elite ? 1u : 0u },
    };
}

U32MapList bountyTalismans(const taskboard::TBountyTaskData& data)
{
    U32MapList rows;
    for (size_t i = 0; i < data.talismans.size(); ++i) {
        const auto& talisman = data.talismans[i];
        const auto path = static_cast<uint8_t>(i);
        const auto level = static_cast<uint8_t>(std::min<uint32_t>(talisman.level, 255));
        const uint32_t current = talismanBonusHundredths(level, path);
        const uint32_t next = level >= talismanMaxLevel(path) || talisman.upgradeCost == 0
            ? 0
            : talismanBonusHundredths(static_cast<uint8_t>(level + 1), path);
        rows.push_back({ { "currentValue", current },
                         { "nextValue", next },
                         { "upgradeCost", talisman.upgradeCost },
                         { "isActiveUpgrade", talisman.upgradable ? 1u : 0u } });
    }
    return rows;
}

U32MapList bountyPreferredSlots(const taskboard::TBountyTaskData& data)
{
    U32MapList rows;
    for (const auto& slot : data.preferredMonsterSlots)
        rows.push_back({ { "slot", static_cast<uint32_t>(slot.index + 1) },
                         { "locked", slot.unlocked ? 0u : 1u },
                         { "preferred", slot.preferredRaceId },
                         { "unwanted", slot.avoidedRaceId },
                         { "price", 0 } });
    return rows;
}

U32Map weeklyHeader(const taskboard::TWeeklyTaskData& w)
{
    const bool hasGeneratedTasks = w.anyCreatureRequiredKills > 0 || w.anyCreatureCurrentKills > 0 || !w.killTasks.empty()
        || !w.deliveryTasks.empty() || w.progressFinished != 0;
    return {
        { "difficulty", hasGeneratedTasks ? static_cast<uint32_t>(w.difficulty) + 1 : 0 },
        { "currentPlayerLevel", tibia::game::localPlayerLevel() },
        { "remainingDays", remainingDaysUntil(w.resetTime) },
        { "totalTaskSlots", w.permanentExpansion ? TASK_BOARD_WEEKLY_EXPANDED_SLOTS : TASK_BOARD_WEEKLY_BASE_SLOTS },
        { "maxExperience", w.rewardExperiencePerKillTask },
        { "maxDeliveryExperience", w.rewardExperiencePerDeliveryTask },
        { "completedKillTasks", w.completedKillTasks },
        { "completedDeliveryTasks", w.completedDeliveryTasks },
        { "pointsEarned", w.rewardHuntingTaskPoints },
        { "soulsealsEarned", w.rewardSoulSeals },
        { "extraSlot", w.permanentExpansion ? 1u : 0u },
        { "unlockedDifficulty", static_cast<uint32_t>(w.maxUnlockedDifficulty) + 1 },
        { "weeklyProgressFinished", static_cast<uint32_t>(w.progressFinished) },
    };
}

U32MapList weeklyItems(const taskboard::TWeeklyTaskData& w)
{
    U32MapList rows;
    for (const auto& d : w.deliveryTasks) {
        const uint32_t claimed = d.state != 0 ? 1u : 0u;
        const bool done = claimed != 0 || (d.requiredAmount > 0 && d.availableAmount >= d.requiredAmount);
        rows.push_back({ { "slotIndex", d.slotIndex },
                         { "itemId", d.objectTypeId },
                         { "current", d.availableAmount },
                         { "total", d.requiredAmount },
                         { "claimed", claimed },
                         { "state", done ? 1u : 0u } });
    }
    return rows;
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
    new TSoulSealsDialogController(this);
    new TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(this);

    connect(taskboard::TTaskboardBountyTasksStorage::instance(), &taskboard::TTaskboardBountyTasksStorage::bountyTaskDataChanged,
            this, [this](const taskboard::TBountyTaskData& data) {
                m_bounty->applyBountyData(bountyHeader(data), taskboardBountyMonsterRows(data), bountyTalismans(data));
                refreshCurrencies();
                m_bounty->applyPreferredData(bountyPreferredSlots(data), allMonsterRaceIds());
            });
    connect(taskboard::TTaskboardWeeklyTasksStorage::instance(), &taskboard::TTaskboardWeeklyTasksStorage::weeklyTaskDataChanged,
            this, [this](const taskboard::TWeeklyTaskData& data) {
                m_weekly->applyWeeklyData(weeklyHeader(data), taskboardWeeklyMonsterRows(data), weeklyItems(data));
                refreshCurrencies();
            });
    connect(taskboard::TTaskboardShopStorage::instance(), &taskboard::TTaskboardShopStorage::shopRewardDataChanged, this, [this]() {
        m_shop->applyShopData(shopOffers(taskboard::TTaskboardShopStorage::instance()->shopRewardData()));
        refreshCurrencies();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() { refreshFromResourceBalance(); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        dialog()->destroy();
    });

    m_notOnMain->setCloseOnButtonClicked(true);
    // The action's parameter picks the tab: 0 bounty tasks, 1 weekly tasks.
    input::setActionHandler(A::OPEN_TASKBOARD_DIALOG, [this](const input::gameactions::TGameActionOpenTaskboardDialog& action) {
        if (action.tab() == 0)
            showDialog(BountyTasks);
        else if (action.tab() == 1)
            showDialog(WeeklyTasks);
        return true;
    });
}

TTaskboardDialogController::~TTaskboardDialogController()
{
    input::setActionHandler(A::OPEN_TASKBOARD_DIALOG, nullptr);
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
    refreshCurrencies();
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

void TTaskboardDialogController::refreshCurrencies()
{
    const QString bounty = formatBalance(resourceBalance(tibia::client::RESOURCE_BOUNTY_POINTS));
    if (bounty != m_bountyPointsString) {
        m_bountyPointsString = bounty;
        Q_EMIT bountyPointsChanged();
    }
    const QString tokens = formatBalance(resourceBalance(tibia::client::RESOURCE_TASK_HUNTING));
    if (tokens != m_preyHuntingTaskTokensString) {
        m_preyHuntingTaskTokensString = tokens;
        Q_EMIT preyHuntingTaskTokensChanged();
    }
    const QString seals = formatBalance(resourceBalance(tibia::client::RESOURCE_SOULSEALS));
    if (seals != m_soulSealsString) {
        m_soulSealsString = seals;
        Q_EMIT soulSealsChanged();
    }
}

void TTaskboardDialogController::refreshFromResourceBalance()
{
    if (m_dialogCreating) {
        m_pendingResourceRefresh = true;
        return;
    }
    refreshCurrencies();
    if (m_bounty)
        m_bounty->refreshBountyPoints();
    if (m_shop)
        m_shop->refreshHuntingTaskPoints();
}

}
