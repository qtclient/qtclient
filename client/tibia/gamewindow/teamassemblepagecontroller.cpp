#include "teamassemblepagecontroller.h"

namespace tibia::gamewindow {

TTeamAssemblePageController::TTeamAssemblePageController(QObject* parent)
    : cip::tibia::gamewindow::TTeamAssemblePageController(parent)
    , m_settings(new teamfinder::TTeamSettings(this))
    , m_activitiesSource(new cip::TActivitiesModel(this))
    , m_activities(new TActivitiesSortProxyModel(m_activitiesSource, this))
    , m_membersSource(new cip::TTeamMembersModel(this))
    , m_members(new TTeamMembersSortProxyModel(m_membersSource, this))
{
    m_teamSettings = m_settings;
    m_activityList = m_activities;
    m_teamMembers = m_members;
    m_hasPremium = true;
    Q_EMIT premiumTimeChanged();
}

bool TTeamAssemblePageController::hasPremium() const
{
    return m_hasPremium;
}

bool TTeamAssemblePageController::assemblyStarted() const
{
    return m_assemblyStarted;
}

QString TTeamAssemblePageController::activityName() const
{
    return m_activityName;
}

bool TTeamAssemblePageController::isInParty() const
{
    return m_isInParty;
}

QObject* TTeamAssemblePageController::teamSettings() const
{
    return m_teamSettings;
}

QObject* TTeamAssemblePageController::activityList() const
{
    return m_activityList;
}

QObject* TTeamAssemblePageController::teamMembers() const
{
    return m_teamMembers;
}

void TTeamAssemblePageController::showDialogTab()
{
    // CIP: reload activity catalog filters; clear stale assembly UI until storage confirms.
    if (m_activities)
        m_activities->resetFilters();
    if (m_assemblyStarted) {
        m_assemblyStarted = false;
        m_activityName.clear();
        Q_EMIT assemblyStartedChanged();
    }
}

void TTeamAssemblePageController::hideDialogTab()
{
}

void TTeamAssemblePageController::onStartAssembleTeamClicked(const int ActivityRowIndex)
{
    Q_UNUSED(ActivityRowIndex);
    // CIP: create listing from selected activity row + teamSettings.
}

void TTeamAssemblePageController::onUpdateTeamSettingsClicked()
{
    // CIP: update active listing from teamSettings.
}

void TTeamAssemblePageController::onUnlistTeamClicked()
{
    // CIP: reset leader listing.
    if (!m_assemblyStarted)
        return;
    m_assemblyStarted = false;
    m_activityName.clear();
    Q_EMIT assemblyStartedChanged();
}

void TTeamAssemblePageController::onEditMemberStatusClicked(const uint CharacterIDRaw, const uint CurrentStatusRaw)
{
    Q_UNUSED(CharacterIDRaw);
    Q_UNUSED(CurrentStatusRaw);
    // CIP: cycle member status Accepted → Invited → Pending → Rejected.
}

void TTeamAssemblePageController::getPremiumClicked()
{
    // CIP: open premium purchase flow.
}

uint TTeamAssemblePageController::getLeaderlevel()
{
    return 1u;
}

void TTeamAssemblePageController::onTeamParametersChanged()
{
    // CIP: teamfinder storage parameters changed.
}

void TTeamAssemblePageController::onTeamMembersChanged()
{
    if (m_members)
        m_members->refreshCounts();
}

void TTeamAssemblePageController::onOwnTeamUpdateAvailable()
{
    // CIP: own-team storage update available.
}

}
