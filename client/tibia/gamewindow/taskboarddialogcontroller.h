#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tibia {
namespace taskboard {
struct TBountyTaskData;
struct TWeeklyTaskData;
}
namespace gamewindow {

/// Rows of the task board storages in the shape the tab controllers and the
/// kill tracker consume.
std::vector<std::map<std::string, uint32_t>> taskboardBountyMonsterRows(const taskboard::TBountyTaskData& data);
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

public Q_SLOTS:
    void showDialog(const DialogTab& Tab) override;
    void closeDialog() override;
    void requestClose() override;
    void requestTabSwitch(const DialogTab& NewTab) override;

private:
    void switchToTab(DialogTab tab);
    void refreshCurrencies();
    void refreshFromResourceBalance();

    TBountyTasksController* m_bounty = nullptr;
    TWeeklyTasksController* m_weekly = nullptr;
    TTaskboardShopController* m_shop = nullptr;
    TMessageDialogController* m_notOnMain = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
    /// True while TaskboardDialog.qml (and nested tab Loaders) are being created.
    /// Resource-balance slots must not emit into incubating QML — that corrupts the heap.
    bool m_dialogCreating = false;
    bool m_pendingResourceRefresh = false;
};

}
}
