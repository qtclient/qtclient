#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
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
/// OPEN_TASKBOARD_DIALOG action (bounty or weekly tab) and fills tabs from
/// the task board storages.
class TTaskboardDialogController final : public TDialogController<cip::tibia::gamewindow::TTaskboardDialogController>
{
    Q_OBJECT
public:
    explicit TTaskboardDialogController(QObject* parent = nullptr);
    ~TTaskboardDialogController() override;

    /// The session's task board currencies.
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
    /// Host: the tabs are not ported with their own session connectors; the
    /// dialog feeds them from the session's task board storages.
    void connectTabsToGameSession();
    void refreshFromResourceBalance();

    TBountyTasksController* m_bounty = nullptr;
    TWeeklyTasksController* m_weekly = nullptr;
    TTaskboardShopController* m_shop = nullptr;
    TMessageDialogController* m_notOnMain = nullptr;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// True while TaskboardDialog.qml (and nested tab Loaders) are being created.
    /// Resource-balance slots must not emit into incubating QML — that corrupts the heap.
    bool m_dialogCreating = false;
    bool m_pendingResourceRefresh = false;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
