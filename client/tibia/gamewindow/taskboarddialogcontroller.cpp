#include "taskboarddialogcontroller.h"

#include "gamewindow/bountytaskscontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/taskboardshopcontroller.h"
#include "gamewindow/weeklytaskscontroller.h"
#include "input/gameactionhandlers.h"

#include <QLocale>

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

template <typename T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

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
    m_rewardShopController = m_shop;
    new TSoulSealsDialogController(this);
    new TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(this);

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBountyTaskData", [this](GameObject*, const Args& args) {
        const auto* header = arg<U32Map>(args, 0);
        const auto* monsters = arg<U32MapList>(args, 1);
        const auto* talismans = arg<U32MapList>(args, 2);
        if (header && monsters && talismans)
            m_bounty->applyBountyData(*header, *monsters, *talismans);
        refreshCurrencies();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBountyPreferredData", [this](GameObject*, const Args& args) {
        const auto* preferred = arg<U32MapList>(args, 0);
        const auto* raceIds = arg<std::vector<uint16_t>>(args, 2);
        if (preferred)
            m_bounty->applyPreferredData(*preferred, raceIds ? *raceIds : std::vector<uint16_t>{});
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onWeeklyTaskData", [this](GameObject*, const Args& args) {
        const auto* header = arg<U32Map>(args, 0);
        const auto* monsters = arg<U32MapList>(args, 1);
        const auto* items = arg<U32MapList>(args, 2);
        if (header && monsters && items)
            m_weekly->applyWeeklyData(*header, *monsters, *items);
        refreshCurrencies();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onTaskHuntingShopData", [this](GameObject*, const Args& args) {
        const auto* offers = arg<StrMapList>(args, 0);
        if (offers)
            m_shop->applyShopData(*offers);
        refreshCurrencies();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() { refreshFromResourceBalance(); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        dialog()->destroy();
    });

    m_notOnMain->setCloseOnButtonClicked(true);
    // The action's parameter picks the tab: 0 bounty tasks, 1 weekly tasks.
    input::setActionHandler(A::OPEN_TASKBOARD_DIALOG, [this](const input::TGameAction& action) {
        if (action.parameter == 0)
            showDialog(BountyTasks);
        else if (action.parameter == 1)
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
