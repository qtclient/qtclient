#pragma once

#include "gamewindow/taskboardmodels.h"

#include <cip_gen.h>

#include <map>
#include <string>
#include <vector>

namespace tibia::gamewindow {

class TTaskboardShopController final : public cip::tibia::gamewindow::TTaskboardShopController
{
    Q_OBJECT
public:
    explicit TTaskboardShopController(QObject* parent = nullptr);

    void applyShopData(const std::vector<std::map<std::string, std::string>>& offers);
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
