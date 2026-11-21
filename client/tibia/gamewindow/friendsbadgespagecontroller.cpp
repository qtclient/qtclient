#include "friendsbadgespagecontroller.h"
#include "game/gamesessionbase.h"

#include "friends/friendsciptypes.h"
#include "protocol/clientmessages.h"

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kLoadBadges = 12;

}

TFriendsBadgesPageController::TFriendsBadgesPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsBadgesPageController(parent)
    , m_model(new TAccountBadgesSortProxyModel(this))
{
    m_badges = m_model;
}

void TFriendsBadgesPageController::updateBadges()
{
    if (friends::TFriendsStorage* storage = friendsStorage())
        m_model->setBadges(storage->badges());
}

TFriendsBadgesPageController::EBadgeFilter TFriendsBadgesPageController::badgeFilter() const
{
    return static_cast<EBadgeFilter>(m_model->badgeFilter());
}

void TFriendsBadgesPageController::setBadgeFilter(const EBadgeFilter filter)
{
    if (!m_model->setBadgeFilter(filter))
        return;
    m_badgeFilter = filter;
    Q_EMIT badgeFilterChanged();
}

QObject* TFriendsBadgesPageController::badges() const
{
    return m_badges;
}

void TFriendsBadgesPageController::showDialogTab()
{
    // Opening the page shows every badge again, follows the stored badges
    // while it is open and asks for the current list.
    setBadgeFilter(ShowAll);
    updateBadges();
    disconnect(m_badgesConnection);
    m_badgesConnection = connect(friendsStorage(), &friends::TFriendsStorage::accountBadgesChanged, this,
                                 &TFriendsBadgesPageController::updateBadges);
    protocol::gameclient::sendFriendSystemAction(kLoadBadges);
}

void TFriendsBadgesPageController::hideDialogTab()
{
    disconnect(m_badgesConnection);
}

void TFriendsBadgesPageController::updateBadgeVisibility(const uint BadgeID, const bool ShouldBeVisibleForOthers)
{
    protocol::gameclient::sendFriendSystemBadgeVisibility(BadgeID, ShouldBeVisibleForOthers);
}


void TFriendsBadgesPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
}

friends::TFriendsStorage* TFriendsBadgesPageController::friendsStorage() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->friendsStorage().get() : nullptr;
}

}
