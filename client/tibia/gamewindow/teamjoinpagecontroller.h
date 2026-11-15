#pragma once

#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

namespace tibia {
namespace game {
class TPremiumTimeStorage;
}
namespace gamewindow {

/// TeamJoinPage.qml (CIP TTeamJoinPageController): the listed teams, apply / withdraw.
class TTeamJoinPageController final : public cip::tibia::gamewindow::TTeamJoinPageController
{
    Q_OBJECT
public:
    explicit TTeamJoinPageController(QObject* parent = nullptr);

    bool hasPremium() const override;
    QObject* teamsList() const override;

    /// Binds to the session's team storage, filters for the own character once, and asks for the list.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// The account's premium state; premiumTimeChanged follows it.
    void setPremiumStorage(game::TPremiumTimeStorage* Storage);
    /// A new game session: the filters start over (and pick up the new character on the next show).
    void setGameSession();

public Q_SLOTS:
    void onJoinLeaveTeamClicked(uint TeamIDRaw, uint CurrentStatusRaw) override;
    void getPremiumClicked() override;
    void onTeamLisUpdateAvailable() override;
    void onTeamsChanged() override;

private:
    void disconnectAll();

    QPointer<game::TPremiumTimeStorage> m_premium;
    QPointer<teamfinder::TTeamFinderStorage> m_storage;
    QList<QMetaObject::Connection> m_connections;
    teamfinder::TTeamfinderActivityNameResolver m_resolver;
    TTeamsListSortProxyModel* m_proxy = nullptr;
};

}
}
