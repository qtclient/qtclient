#include "friends/friendsciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <QCoreApplication>

namespace tibia::friends {

using namespace ::tibia::protobuf::protocol;

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TFriendsAccountSearchStorage::setResult(const ProtocolFriendAccountSearch& result)
{
    m_result = result;
    // No account and no characters: the name matched nobody.
    if (result.account_id() == 0 && result.characters_size() == 0)
        Q_EMIT receivedNoResultsFound();
    else
        Q_EMIT receivedAccountSearchResults();
}

void TFriendsFriendListStorage::setFriends(std::vector<ProtocolFriendListEntry> friends)
{
    m_friends = std::move(friends);
    Q_EMIT friendListChanged();
}

void TFriendsInviteStorage::setInvites(std::vector<ProtocolFriendInvite> invites)
{
    m_invites = std::move(invites);
    Q_EMIT inviteListChanged();
}

void TFriendsInviteStorage::setBlacklist(std::vector<ProtocolFriendBlacklistEntry> blacklist)
{
    m_blacklist = std::move(blacklist);
    Q_EMIT blacklistChanged();
}

TFriendsStorage::TFriendsStorage(QObject* parent)
    : cip::tibia::friends::TFriendsStorage(parent)
    , m_friendList(new TFriendsFriendListStorage(this))
    , m_invites(new TFriendsInviteStorage(this))
    , m_search(new TFriendsAccountSearchStorage(this))
{
}

TFriendsStorage* TFriendsStorage::instance()
{
    static TFriendsStorage* storage = new TFriendsStorage(QCoreApplication::instance());
    return storage;
}

void TFriendsStorage::setBadges(std::vector<ProtocolFriendBadge> badges)
{
    m_badges = std::move(badges);
    Q_EMIT accountBadgesChanged();
}

void TFriendsStorage::setSettings(std::vector<ProtocolFriendSetting> settings)
{
    m_settings = std::move(settings);
    Q_EMIT visibilitySettingsChanged();
}

TFriendsProtocolMessageHandler* TFriendsProtocolMessageHandler::instance()
{
    static TFriendsProtocolMessageHandler* handler = new TFriendsProtocolMessageHandler(QCoreApplication::instance());
    return handler;
}

void TFriendsProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    if (opcode != 116)
        return;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || !shadow->HasExtension(GameserverMessageFriendSystemData::gameserver_message_friend_system_data))
        return;
    instance()->handleFriendSystemDataMessage(
        shadow->GetExtension(GameserverMessageFriendSystemData::gameserver_message_friend_system_data));
}

void TFriendsProtocolMessageHandler::handleFriendSystemDataMessage(const GameserverMessageFriendSystemData& message)
{
    TFriendsStorage* storage = TFriendsStorage::instance();
    switch (static_cast<EFriendSystemData>(message.data_type())) {
    case EFriendSystemData::Error:
        if (message.error_code() == 0)
            Q_EMIT cannotUseFriendSystemError();
        else if (message.error_code() == 1)
            Q_EMIT genericFriendSystemError(QString::fromStdString(message.error_message()));
        else
            Q_EMIT errorReceived();
        break;
    case EFriendSystemData::InviteNotice:
        Q_EMIT newFriendInviteReceived();
        break;
    case EFriendSystemData::FriendshipNotice:
        Q_EMIT newFriendshipReceived();
        break;
    case EFriendSystemData::FriendList:
        storage->friendList()->setFriends({ message.friends().begin(), message.friends().end() });
        break;
    case EFriendSystemData::Invites:
        storage->inviteStorage()->setInvites({ message.invites().begin(), message.invites().end() });
        break;
    case EFriendSystemData::Blacklist:
        storage->inviteStorage()->setBlacklist({ message.blacklist().begin(), message.blacklist().end() });
        break;
    case EFriendSystemData::AccountSearch:
        storage->accountSearch()->setResult(message.account_search());
        break;
    case EFriendSystemData::Badges:
        storage->setBadges({ message.badges().begin(), message.badges().end() });
        break;
    case EFriendSystemData::Settings:
        storage->setSettings({ message.settings().begin(), message.settings().end() });
        break;
    }
    // Product pages (IdleOT social) still listen on the notify bus.
    tibia::game::notify("onFriendSystemData", static_cast<int>(message.data_type()));
}

}
