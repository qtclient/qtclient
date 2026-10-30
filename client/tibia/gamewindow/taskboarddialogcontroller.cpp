#include "taskboarddialogcontroller.h"

#include "gamewindow/bountytaskscontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/taskboardshopcontroller.h"
#include "gamewindow/weeklytaskscontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QLocale>

#include <any>
#include <map>
#include <string>
#include <vector>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using U32Map = std::map<std::string, uint32_t>;
using U32MapList = std::vector<U32Map>;
using StrMap = std::map<std::string, std::string>;
using StrMapList = std::vector<StrMap>;

uint64_t resourceBalance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

QString formatBalance(const uint64_t value)
{
    return QLocale::system().toString(static_cast<qulonglong>(value));
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
{
    m_bountyTaskController = m_bounty;
    m_weeklyTaskController = m_weekly;
    m_rewardShopController = m_shop;
    new TSoulSealsDialogController(this);
    new TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(this);

    m_hooks.emplace_back("g_game", "onBountyTaskData", [this](LuaObject*, const Args& args) {
        const auto* header = arg<U32Map>(args, 0);
        const auto* monsters = arg<U32MapList>(args, 1);
        const auto* talismans = arg<U32MapList>(args, 2);
        if (header && monsters && talismans)
            m_bounty->applyBountyData(*header, *monsters, *talismans);
        refreshCurrencies();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onBountyPreferredData", [this](LuaObject*, const Args& args) {
        const auto* preferred = arg<U32MapList>(args, 0);
        const auto* raceIds = arg<std::vector<uint16_t>>(args, 2);
        if (preferred)
            m_bounty->applyPreferredData(*preferred, raceIds ? *raceIds : std::vector<uint16_t>{});
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onWeeklyTaskData", [this](LuaObject*, const Args& args) {
        const auto* header = arg<U32Map>(args, 0);
        const auto* monsters = arg<U32MapList>(args, 1);
        const auto* items = arg<U32MapList>(args, 2);
        if (header && monsters && items)
            m_weekly->applyWeeklyData(*header, *monsters, *items);
        refreshCurrencies();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onTaskHuntingShopData", [this](LuaObject*, const Args& args) {
        const auto* offers = arg<StrMapList>(args, 0);
        if (offers)
            m_shop->applyShopData(*offers);
        refreshCurrencies();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args&) {
        refreshCurrencies();
        m_bounty->refreshBountyPoints();
        m_shop->refreshHuntingTaskPoints();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        dialog()->destroy();
        return std::any{};
    });

    input::setActionHandler(A::OPEN_TASKBOARD_DIALOG, [this](const input::TGameAction&) {
        openOrUpdate();
        return true;
    });
}

TTaskboardDialogController::~TTaskboardDialogController()
{
    input::setActionHandler(A::OPEN_TASKBOARD_DIALOG, nullptr);
}

bool TTaskboardDialogController::isOpen() const { return dialog()->isShown(); }

void TTaskboardDialogController::openOrUpdate()
{
    if (isOpen()) {
        closeDialog();
        return;
    }
    showDialog(BountyTasks);
}

void TTaskboardDialogController::showDialog(const DialogTab& Tab)
{
    const DialogTab tab = Tab == NoTabInitialState ? BountyTasks : Tab;
    if (m_dialogTab != tab) {
        m_dialogTab = tab;
        Q_EMIT dialogTabChanged();
    }
    refreshCurrencies();
    dialog()->show();
    requestDataForTab(tab);
}

void TTaskboardDialogController::closeDialog() { dialog()->close(); }

void TTaskboardDialogController::requestClose() { closeDialog(); }

void TTaskboardDialogController::requestTabSwitch(const DialogTab& NewTab)
{
    if (NewTab == NoTabInitialState || NewTab == m_dialogTab)
        return;
    m_dialogTab = NewTab;
    Q_EMIT dialogTabChanged();
    requestDataForTab(NewTab);
}

void TTaskboardDialogController::requestDataForTab(const DialogTab tab)
{
    if (!g_game.canPerformGameAction())
        return;
    switch (tab) {
    case BountyTasks:
        m_bounty->showDialogTab();
        break;
    case WeeklyTasks:
        m_weekly->showDialogTab();
        break;
    case Shop:
        m_shop->showDialogTab();
        break;
    case NoTabInitialState:
        break;
    }
}

void TTaskboardDialogController::refreshCurrencies()
{
    const QString bounty = formatBalance(resourceBalance(Otc::RESOURCE_BOUNTY_POINTS));
    if (bounty != m_bountyPointsString) {
        m_bountyPointsString = bounty;
        Q_EMIT bountyPointsChanged();
    }
    const QString tokens = formatBalance(resourceBalance(Otc::RESOURCE_TASK_HUNTING));
    if (tokens != m_preyHuntingTaskTokensString) {
        m_preyHuntingTaskTokensString = tokens;
        Q_EMIT preyHuntingTaskTokensChanged();
    }
    const QString seals = formatBalance(resourceBalance(Otc::RESOURCE_SOULSEALS));
    if (seals != m_soulSealsString) {
        m_soulSealsString = seals;
        Q_EMIT soulSealsChanged();
    }
}

}
