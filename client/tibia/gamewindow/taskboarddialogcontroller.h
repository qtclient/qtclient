#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

namespace tibia::gamewindow {

class TBountyTasksController;
class TWeeklyTasksController;
class TTaskboardShopController;

/// TaskboardDialog.qml (CIP: TTaskboardDialogController). Opens from the
/// button bar / side rail OPEN_TASKBOARD_DIALOG action and fills tabs from
/// onBountyTaskData / onWeeklyTaskData / onTaskHuntingShopData.
class TTaskboardDialogController final : public TDialogController<cip::tibia::gamewindow::TTaskboardDialogController>
{
    Q_OBJECT
public:
    explicit TTaskboardDialogController(QObject* parent = nullptr);
    ~TTaskboardDialogController() override;

    void openOrUpdate();
    bool isOpen() const;

public Q_SLOTS:
    void showDialog(const DialogTab& Tab) override;
    void closeDialog() override;
    void requestClose() override;
    void requestTabSwitch(const DialogTab& NewTab) override;

private:
    void requestDataForTab(DialogTab tab);
    void refreshCurrencies();

    TBountyTasksController* m_bounty = nullptr;
    TWeeklyTasksController* m_weekly = nullptr;
    TTaskboardShopController* m_shop = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
