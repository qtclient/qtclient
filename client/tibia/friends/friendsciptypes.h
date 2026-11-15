#pragma once

// CIP friends:: storages and the FriendSystemData (S 0x74) handler.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QObject>
#include <QString>

#include <vector>

namespace tibia {
namespace friends {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP friends::TFriendsAccountSearchStorage: the last account search result.
class TFriendsAccountSearchStorage final : public cip::tibia::friends::TFriendsAccountSearchStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsAccountSearchStorage::TFriendsAccountSearchStorage;

    const ::tibia::protobuf::protocol::ProtocolFriendAccountSearch& result() const { return m_result; }
    void setResult(const ::tibia::protobuf::protocol::ProtocolFriendAccountSearch& result);

private:
    ::tibia::protobuf::protocol::ProtocolFriendAccountSearch m_result;
};

/// CIP friends::TFriendsFriendListStorage.
class TFriendsFriendListStorage final : public cip::tibia::friends::TFriendsFriendListStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsFriendListStorage::TFriendsFriendListStorage;

    const std::vector<::tibia::protobuf::protocol::ProtocolFriendListEntry>& friends() const { return m_friends; }
    void setFriends(std::vector<::tibia::protobuf::protocol::ProtocolFriendListEntry> friends);

private:
    std::vector<::tibia::protobuf::protocol::ProtocolFriendListEntry> m_friends;
};

/// CIP friends::TFriendsInviteStorage: received and sent invites, and the blacklist.
class TFriendsInviteStorage final : public cip::tibia::friends::TFriendsInviteStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsInviteStorage::TFriendsInviteStorage;

    const std::vector<::tibia::protobuf::protocol::ProtocolFriendInvite>& invites() const { return m_invites; }
    const std::vector<::tibia::protobuf::protocol::ProtocolFriendBlacklistEntry>& blacklist() const { return m_blacklist; }
    void setInvites(std::vector<::tibia::protobuf::protocol::ProtocolFriendInvite> invites);
    void setBlacklist(std::vector<::tibia::protobuf::protocol::ProtocolFriendBlacklistEntry> blacklist);

private:
    std::vector<::tibia::protobuf::protocol::ProtocolFriendInvite> m_invites;
    std::vector<::tibia::protobuf::protocol::ProtocolFriendBlacklistEntry> m_blacklist;
};

/// CIP friends::TFriendsStorage: the player's badges and visibility settings,
/// and the owner of the list storages.
class TFriendsStorage final : public cip::tibia::friends::TFriendsStorage
{
    Q_OBJECT
public:
    explicit TFriendsStorage(QObject* parent = nullptr);

    /// The session's friends storage (created on first use).
    static TFriendsStorage* instance();

    TFriendsFriendListStorage* friendList() const { return m_friendList; }
    TFriendsInviteStorage* inviteStorage() const { return m_invites; }
    TFriendsAccountSearchStorage* accountSearch() const { return m_search; }

    const std::vector<::tibia::protobuf::protocol::ProtocolFriendBadge>& badges() const { return m_badges; }
    const std::vector<::tibia::protobuf::protocol::ProtocolFriendSetting>& settings() const { return m_settings; }
    void setBadges(std::vector<::tibia::protobuf::protocol::ProtocolFriendBadge> badges);
    void setSettings(std::vector<::tibia::protobuf::protocol::ProtocolFriendSetting> settings);

private:
    TFriendsFriendListStorage* m_friendList;
    TFriendsInviteStorage* m_invites;
    TFriendsAccountSearchStorage* m_search;
    std::vector<::tibia::protobuf::protocol::ProtocolFriendBadge> m_badges;
    std::vector<::tibia::protobuf::protocol::ProtocolFriendSetting> m_settings;
};

/// CIP friends::TFriendsProtocolMessageHandler: applies FriendSystemData to
/// the storages and reports errors and notices.
class TFriendsProtocolMessageHandler final : public cip::tibia::friends::TFriendsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsProtocolMessageHandler::TFriendsProtocolMessageHandler;

    static TFriendsProtocolMessageHandler* instance();

    /// FriendSystemData from the native message.
    static void onServerMessageEnqueued(int opcode);

    void handleFriendSystemDataMessage(const ::tibia::protobuf::protocol::GameserverMessageFriendSystemData& message);
};

/// The FriendSystemData data types (CIP's switch in handleFriendSystemDataMessage).
enum class EFriendSystemData : int {
    Error = 0,
    InviteNotice = 1,
    FriendList = 2,
    Invites = 3,
    Blacklist = 4,
    AccountSearch = 5,
    Badges = 6,
    FriendshipNotice = 7,
    Settings = 8,
};

}
}
