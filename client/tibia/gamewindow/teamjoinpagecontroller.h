#pragma once

#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

/// TeamJoinPage.qml (CIP TTeamJoinPageController).
class TTeamJoinPageController final : public cip::tibia::gamewindow::TTeamJoinPageController
{
    Q_OBJECT
public:
    explicit TTeamJoinPageController(QObject* parent = nullptr);

    bool hasPremium() const override;
    QObject* teamsList() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

public Q_SLOTS:
    void onJoinLeaveTeamClicked(uint TeamIDRaw, uint CurrentStatusRaw) override;
    void getPremiumClicked() override;
    void onTeamLisUpdateAvailable() override;
    void onTeamsChanged() override;

private:
    cip::TTeamsListModel* m_source = nullptr;
    TTeamsListSortProxyModel* m_proxy = nullptr;
};

}
}
