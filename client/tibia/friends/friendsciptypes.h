#pragma once

// CIP friends:: data types, storages and the FriendSystemData (S 0x74) handler.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace tibia {
namespace friends {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP friends::TAccountCharacterListEntry: one character of an account (a
/// friend's main character, or a character of an account search result).
struct TAccountCharacterListEntry
{
    bool isMainCharacter = false;
    uint32_t accountId = 0;
    uint32_t characterId = 0;
    QString name;
    QString title;
    QString world;
    QString vocation;
    uint16_t level = 0;
    uint16_t outfitId = 0;
    uint8_t headColor = 0;
    uint8_t torsoColor = 0;
    uint8_t legsColor = 0;
    uint8_t detailColor = 0;
    bool firstAddon = false;
    bool secondAddon = false;
    bool isOnline = false;
};

/// CIP friends::TFriendList.
struct TFriendList
{
    /// CIP friends::TFriendList::TFriendEntry: a friend's main character and
    /// the friend group (1 contact, 2 friend, 3 close friend).
    struct TFriendEntry
    {
        TAccountCharacterListEntry character;
        int friendGroup = 0;
    };
};

/// CIP friends::TInviteList: the invites the player sent and received.
struct TInviteList
{
    /// CIP friends::TInviteList::TInviteListEntry.
    struct TInviteListEntry
    {
        bool isMainCharacter = false;
        uint32_t accountId = 0;
        uint32_t characterId = 0;
        QString name;
        QString title;
        QString inviterName;
        uint32_t timestamp = 0;
    };

    std::vector<TInviteListEntry> sent;
    std::vector<TInviteListEntry> received;
};

/// CIP friends::TBlacklist.
struct TBlacklist
{
    /// CIP friends::TBlacklist::TBlacklistEntry.
    struct TBlacklistEntry
    {
        bool isMainCharacter = false;
        uint32_t accountId = 0;
        uint32_t characterId = 0;
        QString name;
        uint32_t timestamp = 0;
    };
};

/// CIP friends::TAccountBadge.
struct TAccountBadge
{
    TAccountBadge() = default;
    /// The name keeps "Grade N" on one line ("Grade " becomes "Grade&nbsp;").
    TAccountBadge(uint32_t id, const QString& name, QString description, bool unlocked, bool visible);

    uint32_t id = 0;
    QString name;
    QString description;
    bool unlocked = false;
    bool visible = false;
};

/// CIP friends::TVisibilitySetting: what one friend group may see.
struct TVisibilitySetting
{
    int friendGroup = 0;
    bool showCharacterInfo = false;
    bool showAccountInfo = false;
    bool allowInspect = false;
};

/// The account search result held by TFriendsAccountSearchStorage.
struct TAccountSearchResult
{
    bool isOnline = false;
    bool isPremium = false;
    bool isFriendOrInvited = false;
    bool isOnBlacklist = false;
    bool showAccountInfo = false;
    /// The first character is the account's main character.
    bool hasMainCharacter = false;
    uint32_t accountId = 0;
    uint32_t lastGameLogin = 0;
    QString loyaltyTitle;
    std::vector<TAccountBadge> badges;
    std::vector<TAccountCharacterListEntry> characters;
};

/// CIP friends::TFriendsAccountSearchStorage: the last account search result.
class TFriendsAccountSearchStorage final : public cip::tibia::friends::TFriendsAccountSearchStorage
{
    Q_OBJECT
public:
    explicit TFriendsAccountSearchStorage(QObject* parent = nullptr);

    const TAccountSearchResult& result() const { return m_result; }
    void setResult(TAccountSearchResult result);
    /// Empties the result (a failed search) and reports both a result and no results.
    void clear();

private:
    /// Moves the main character to the front and records whether there is one.
    void moveMainCharacterToFront();

    TAccountSearchResult m_result;
};

/// CIP friends::TFriendsFriendListStorage.
class TFriendsFriendListStorage final : public cip::tibia::friends::TFriendsFriendListStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsFriendListStorage::TFriendsFriendListStorage;

    const std::vector<TFriendList::TFriendEntry>& friendList() const { return m_friendList; }
    void setFriendList(const std::vector<TFriendList::TFriendEntry>& friendList);
    /// Drops the account's entry (after an unfriend) and reports the change.
    void removeFriend(uint32_t accountId);
    /// Moves the account into another friend group and reports the change.
    void setFriendGroup(uint32_t accountId, int friendGroup);
    /// The account's friend group, 0 when it is not on the list.
    int friendGroupOf(uint32_t accountId) const;

private:
    std::vector<TFriendList::TFriendEntry> m_friendList;
};

/// CIP friends::TFriendsInviteStorage: sent and received invites, and the blacklist.
class TFriendsInviteStorage final : public cip::tibia::friends::TFriendsInviteStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsInviteStorage::TFriendsInviteStorage;

    const TInviteList& inviteList() const { return m_inviteList; }
    const std::vector<TBlacklist::TBlacklistEntry>& blacklist() const { return m_blacklist; }
    void setInviteList(TInviteList inviteList);
    void setBlacklist(std::vector<TBlacklist::TBlacklistEntry> blacklist);

private:
    TInviteList m_inviteList;
    std::vector<TBlacklist::TBlacklistEntry> m_blacklist;
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
    TFriendsAccountSearchStorage* accountSearch() const { return m_accountSearch; }
    TFriendsInviteStorage* inviteStorage() const { return m_invites; }

    const std::vector<TAccountBadge>& badges() const { return m_badges; }
    const std::vector<TVisibilitySetting>& settings() const { return m_settings; }
    void setBadges(std::vector<TAccountBadge> badges);
    void setSettings(std::vector<TVisibilitySetting> settings);

private:
    TFriendsFriendListStorage* m_friendList;
    TFriendsAccountSearchStorage* m_accountSearch;
    TFriendsInviteStorage* m_invites;
    std::vector<TAccountBadge> m_badges;
    std::vector<TVisibilitySetting> m_settings;
};

/// CIP friends::TFriendsProtocolMessageHandler: turns FriendSystemData into
/// the storages' types and reports errors and notices.
class TFriendsProtocolMessageHandler final : public cip::tibia::friends::TFriendsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsProtocolMessageHandler::TFriendsProtocolMessageHandler;

    static TFriendsProtocolMessageHandler* instance();

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

/// A friend group (or visibility setting type) from the protocol: 0 to 3; any
/// other value throws std::runtime_error.
int friendGroupFromProtocol(uint32_t value);
/// A friend group for the protocol (same range check).
uint32_t friendGroupToProtocol(int friendGroup);

}
}
