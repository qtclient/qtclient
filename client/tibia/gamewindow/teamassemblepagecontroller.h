#pragma once

#include "teamfinder/teamfinderciptypes.h"
#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>

namespace tibia {
namespace game {
class TPremiumTimeStorage;
}
namespace gamewindow {

class TMessageDialogController;

/// TeamAssemblePage.qml (CIP TTeamAssemblePageController): list the own team and manage its members.
class TTeamAssemblePageController final : public cip::tibia::gamewindow::TTeamAssemblePageController
{
    Q_OBJECT
public:
    explicit TTeamAssemblePageController(QObject* parent = nullptr);

    bool hasPremium() const override;
    /// The own team is listed (it has an activity).
    bool assemblyStarted() const override;
    QString activityName() const override;
    bool isInParty() const override;
    QObject* teamSettings() const override;
    QObject* activityList() const override;
    QObject* teamMembers() const override;

    /// Binds to the session's team storage, shows the own team and asks the server for it.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// The account's premium state; premiumTimeChanged follows it.
    void setPremiumStorage(game::TPremiumTimeStorage* Storage);

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
    void showErrors(const QStringList& Errors);
    void disconnectAll();

    TMessageDialogController* m_errorMessage = nullptr;
    QPointer<game::TPremiumTimeStorage> m_premium;
    QPointer<teamfinder::TTeamFinderStorage> m_storage;
    QList<QMetaObject::Connection> m_connections;
    teamfinder::TTeamSettings* m_settings = nullptr;
    TActivitiesSortProxyModel* m_activities = nullptr;
    TTeamMembersSortProxyModel* m_members = nullptr;
};

}
}
