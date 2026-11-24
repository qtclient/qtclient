#include "taskboarddialogcontroller.h"

#include "gamewindow/bountytaskscontroller.h"
#include "gamewindow/messagedialogcontroller.h"
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

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using U32Map = std::map<std::string, uint32_t>;
using U32MapList = std::vector<U32Map>;
using StrMap = std::map<std::string, std::string>;
using StrMapList = std::vector<StrMap>;


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

TTaskboardDialogController::TTaskboardDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem)
    : TDialogController(QStringLiteral("taskboard/TaskboardDialog.qml"), engine, dialogParentItem)
    , m_bounty(new TBountyTasksController(engine, dialogParentItem, this))
    , m_weekly(new TWeeklyTasksController(this))
    , m_shop(new TTaskboardShopController(engine, dialogParentItem, this))
    , m_notOnMain(new TMessageDialogController(engine, dialogParentItem, this))
{
    m_bountyTaskController = m_bounty;
    m_weeklyTaskController = m_weekly;
    m_rewardShopController = m_shop;
    m_notOnMain->setCloseOnButtonClicked(true);
    connect(m_weekly, &TWeeklyTasksController::publishGameAction, this, &TTaskboardDialogController::publishGameAction);
}

QObject* TTaskboardDialogController::bountyTaskController() const { return m_bounty; }
QObject* TTaskboardDialogController::weeklyTaskController() const { return m_weekly; }
QObject* TTaskboardDialogController::rewardShopController() const { return m_shop; }

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
    m_shop->connectToGameSession(m_gameSession);
}

QString TTaskboardDialogController::bountyPointsString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return QStringLiteral("0");
    return QLocale(QLocale::English).toString(static_cast<qlonglong>(session->taskboardCurrenciesStorage()->bountyPoints()));
}

QString TTaskboardDialogController::preyHuntingTaskTokensString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return QStringLiteral("0");
    return QLocale(QLocale::English)
        .toString(static_cast<qulonglong>(static_cast<uint32_t>(session->taskboardCurrenciesStorage()->preyHuntingTaskTokensAmount())));
}

QString TTaskboardDialogController::soulSealsString() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return QStringLiteral("0");
    return QLocale(QLocale::English).toString(static_cast<qlonglong>(session->taskboardCurrenciesStorage()->soulSealsAmount()));
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
    dialog()->show();
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

/// The task board action's parameter picks the tab: 0 bounty tasks, 1 weekly tasks.
void TTaskboardDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != A::OPEN_TASKBOARD_DIALOG)
        return;
    const auto* action = input::gameActionAs<input::gameactions::TGameActionOpenTaskboardDialog>(pGameAction);
    if (!action)
        return;
    if (action->tab() == 0)
        showDialog(BountyTasks);
    else if (action->tab() == 1)
        showDialog(WeeklyTasks);
}

}
