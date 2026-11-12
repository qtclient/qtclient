#pragma once

#include "teamfinder/teamfinderciptypes.h"
#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

/// TeamAssemblePage.qml (CIP TTeamAssemblePageController).
class TTeamAssemblePageController final : public cip::tibia::gamewindow::TTeamAssemblePageController
{
    Q_OBJECT
public:
    explicit TTeamAssemblePageController(QObject* parent = nullptr);

    bool hasPremium() const override;
    bool assemblyStarted() const override;
    QString activityName() const override;
    bool isInParty() const override;
    QObject* teamSettings() const override;
    QObject* activityList() const override;
    QObject* teamMembers() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

public Q_SLOTS:
    void onStartAssembleTeamClicked(int ActivityRowIndex) override;
    void onUpdateTeamSettingsClicked() override;
    void onUnlistTeamClicked() override;
    void onEditMemberStatusClicked(uint CharacterIDRaw, uint CurrentStatusRaw) override;
    void getPremiumClicked() override;
    uint getLeaderlevel() override;
    void onTeamParametersChanged() override;
    void onTeamMembersChanged() override;
    void onOwnTeamUpdateAvailable() override;

private:
    teamfinder::TTeamSettings* m_settings = nullptr;
    cip::TActivitiesModel* m_activitiesSource = nullptr;
    TActivitiesSortProxyModel* m_activities = nullptr;
    cip::TTeamMembersModel* m_membersSource = nullptr;
    TTeamMembersSortProxyModel* m_members = nullptr;
};

}
}
