#include "teamjoinpagecontroller.h"

namespace tibia::gamewindow {

TTeamJoinPageController::TTeamJoinPageController(QObject* parent)
    : cip::tibia::gamewindow::TTeamJoinPageController(parent)
    , m_source(new cip::TTeamsListModel(this))
    , m_proxy(new TTeamsListSortProxyModel(m_source, this))
{
    m_teamsList = m_proxy;
    m_hasPremium = true;
    Q_EMIT premiumTimeChanged();
}

bool TTeamJoinPageController::hasPremium() const
{
    return m_hasPremium;
}

QObject* TTeamJoinPageController::teamsList() const
{
    return m_teamsList;
}

void TTeamJoinPageController::showDialogTab()
{
    // CIP: refresh join list + restore filter defaults when the tab opens.
    if (m_proxy)
        m_proxy->resetFilters();
}

void TTeamJoinPageController::hideDialogTab()
{
}

void TTeamJoinPageController::onJoinLeaveTeamClicked(const uint TeamIDRaw, const uint CurrentStatusRaw)
{
    Q_UNUSED(TeamIDRaw);
    Q_UNUSED(CurrentStatusRaw);
    // CIP: join (status 0) / leave team, then refresh list.
}

void TTeamJoinPageController::getPremiumClicked()
{
    // CIP: open premium purchase flow.
}

void TTeamJoinPageController::onTeamLisUpdateAvailable()
{
    // CIP: team-list storage notified an update is available.
}

void TTeamJoinPageController::onTeamsChanged()
{
    // CIP: rebuild proxy source from teamfinder storage.
}

}
