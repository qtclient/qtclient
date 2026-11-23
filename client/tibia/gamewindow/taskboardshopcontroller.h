#pragma once

#include "taskboard/taskboardrewardlistmodel.h"

#include <cip_gen.h>

#include <map>
#include <string>
#include <vector>

namespace tibia {
namespace gamewindow {

class TTaskboardShopController final : public cip::tibia::gamewindow::TTaskboardShopController
{
    Q_OBJECT
public:
    explicit TTaskboardShopController(QObject* parent = nullptr);

    /// Takes the shop storage's offers into the reward list.
    void setRewards(const std::vector<taskboard::TShopReward>& rewards);
    void refreshHuntingTaskPoints();

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
    void updateRewardList() override;
    void requestBuyOffer(int Index) override;

private:
    void syncPlayerColors();

    taskboard::TTaskboardRewardListModel* m_rewardsModel = nullptr;
};

}
}
