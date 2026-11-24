#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
class TPlayerData;
}
namespace input {
class TGameActionMapper;
}
namespace shared {
class IClockService;
}
namespace taskboard {
struct TWeeklyTaskData;
}
namespace gamewindow {

/// Rows of the task board storages in the shape the tab controllers and the
/// kill tracker consume.
std::vector<std::map<std::string, uint32_t>> taskboardWeeklyMonsterRows(const taskboard::TWeeklyTaskData& data);
bool taskboardWeeklyActive(const taskboard::TWeeklyTaskData& data);

class TBountyTasksController;
class TMessageDialogController;
class TWeeklyTasksController;
class TTaskboardShopController;

/// TaskboardDialog.qml (CIP: TTaskboardDialogController). Opens from the
/// OPEN_TASKBOARD_DIALOG action (bounty or weekly tab); its bounty, weekly
/// and shop tabs follow the session themselves.
class TTaskboardDialogController final : public TDialogController<cip::tibia::gamewindow::TTaskboardDialogController>
{
    Q_OBJECT
public:
    /// CipSoft also passes the game window's mouse mapper and clock on to the
    /// weekly tab; qtclient's weekly tab follows the control scheme itself.
    TTaskboardDialogController(std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                               std::shared_ptr<shared::IClockService> ClockService, QQmlEngine* engine,
                               QQuickItem* dialogParentItem);
    ~TTaskboardDialogController() override;

    QObject* bountyTaskController() const override;
    QObject* weeklyTaskController() const override;
    QObject* rewardShopController() const override;
    DialogTab dialogTab() const override { return m_dialogTab; }
    /// The session's task board currencies in English number format; "0"
    /// without a session.
    QString bountyPointsString() const override;
    QString preyHuntingTaskTokensString() const override;
    QString soulSealsString() const override;
    /// Closes the dialog and follows the session's task board currencies; its
    /// tabs follow the session too.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    void showDialog(const DialogTab& Tab) override;
    void closeDialog() override;
    void requestClose() override;
    void requestTabSwitch(const DialogTab& NewTab) override;

private:
    void switchToTab(DialogTab tab);
    /// The bounty, weekly and shop tabs take the session.
    void connectTabsToGameSession();

    TBountyTasksController* m_bounty = nullptr;
    TWeeklyTasksController* m_weekly = nullptr;
    TTaskboardShopController* m_shop = nullptr;
    TMessageDialogController* m_notOnMain = nullptr;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's player (the dialog opens once it reached the main land).
    std::shared_ptr<game::TPlayerData> m_pPlayerData;
    /// The weekly tab's mouse mapping and clock (CipSoft hands them to the weekly tasks controller).
    std::shared_ptr<input::TGameActionMapper> m_gameActionMapper;
    std::shared_ptr<shared::IClockService> m_clockService;

public Q_SLOTS:
    /// The router's game actions: OPEN_TASKBOARD_DIALOG opens its tab.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
};

}
}
