#include "friends/friendsciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <QCoreApplication>

#include <algorithm>
#include <stdexcept>

namespace tibia::friends {

using namespace ::tibia::protobuf::protocol;

namespace {

QString toQString(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// A character entry from the protocol. The third string on the wire is the
/// world and the fourth the vocation; the proto names those two fields the
/// other way round.
TAccountCharacterListEntry characterEntry(const bool isMainCharacter, const uint32_t accountId, const uint32_t characterId,
                                          const std::string& name, const std::string& title, const std::string& world,
                                          const std::string& vocation, const uint32_t level, const ProtocolOutfit& outfit,
                                          const bool isOnline)
{
    TAccountCharacterListEntry entry;
    entry.isMainCharacter = isMainCharacter;
    entry.accountId = accountId;
    entry.characterId = characterId;
    entry.name = toQString(name);
    entry.title = toQString(title);
    entry.world = toQString(world);
    entry.vocation = toQString(vocation);
    entry.level = static_cast<uint16_t>(level);
    entry.outfitId = static_cast<uint16_t>(outfit.look_type());
    entry.headColor = static_cast<uint8_t>(outfit.head());
    entry.torsoColor = static_cast<uint8_t>(outfit.body());
    entry.legsColor = static_cast<uint8_t>(outfit.legs());
    entry.detailColor = static_cast<uint8_t>(outfit.feet());
    entry.firstAddon = (outfit.addons() & 1) != 0;
    entry.secondAddon = (outfit.addons() & 2) != 0;
    entry.isOnline = isOnline;
    return entry;
}

}

int friendGroupFromProtocol(const uint32_t value)
{
    if (value > 3)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(value);
}

uint32_t friendGroupToProtocol(const int friendGroup)
{
    if (friendGroup < 0 || friendGroup > 3)
        throw std::runtime_error("unhandled enum value");
    return static_cast<uint32_t>(friendGroup);
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

TAccountBadge::TAccountBadge(const uint32_t id, const QString& name, QString description, const bool unlocked, const bool visible)
    : id(id)
    , name(QString(name).replace(QStringLiteral("Grade "), QStringLiteral("Grade&nbsp;")))
    , description(std::move(description))
    , unlocked(unlocked)
    , visible(visible)
{
}

TFriendsAccountSearchStorage::TFriendsAccountSearchStorage(QObject* parent)
    : cip::tibia::friends::TFriendsAccountSearchStorage(parent)
{
}

void TFriendsAccountSearchStorage::moveMainCharacterToFront()
{
    auto& characters = m_result.characters;
    if (characters.empty())
        return;
    std::nth_element(characters.begin(), characters.begin(), characters.end(),
                     [](const TAccountCharacterListEntry& left, const TAccountCharacterListEntry& right) {
                         return left.isMainCharacter && !right.isMainCharacter;
                     });
    if (characters.front().isMainCharacter)
        m_result.hasMainCharacter = true;
}

void TFriendsAccountSearchStorage::setResult(TAccountSearchResult result)
{
    m_result = std::move(result);
    moveMainCharacterToFront();
    Q_EMIT receivedAccountSearchResults();
}

void TFriendsAccountSearchStorage::clear()
{
    m_result = TAccountSearchResult();
    moveMainCharacterToFront();
    Q_EMIT receivedAccountSearchResults();
    Q_EMIT receivedNoResultsFound();
}

void TFriendsFriendListStorage::setFriendList(const std::vector<TFriendList::TFriendEntry>& friendList)
{
    if (&m_friendList != &friendList)
        m_friendList = friendList;
    Q_EMIT friendListChanged();
}

void TFriendsFriendListStorage::removeFriend(const uint32_t accountId)
{
    for (auto it = m_friendList.begin(); it != m_friendList.end(); ++it) {
        if (it->character.accountId == accountId) {
            m_friendList.erase(it);
            break;
        }
    }
    Q_EMIT friendListChanged();
}

void TFriendsFriendListStorage::setFriendGroup(const uint32_t accountId, const int friendGroup)
{
    for (auto& entry : m_friendList) {
        if (entry.character.accountId == accountId) {
            entry.friendGroup = friendGroup;
            break;
        }
    }
    Q_EMIT friendListChanged();
}

int TFriendsFriendListStorage::friendGroupOf(const uint32_t accountId) const
{
    for (const auto& entry : m_friendList) {
        if (entry.character.accountId == accountId)
            return entry.friendGroup;
    }
    return 0;
}

void TFriendsInviteStorage::setInviteList(TInviteList inviteList)
{
    m_inviteList.sent = std::move(inviteList.sent);
    m_inviteList.received = std::move(inviteList.received);
    Q_EMIT inviteListChanged();
}

void TFriendsInviteStorage::setBlacklist(std::vector<TBlacklist::TBlacklistEntry> blacklist)
{
    m_blacklist = std::move(blacklist);
    Q_EMIT blacklistChanged();
}

TFriendsStorage::TFriendsStorage(QObject* parent)
    : cip::tibia::friends::TFriendsStorage(parent)
    , m_friendList(new TFriendsFriendListStorage(this))
    , m_accountSearch(new TFriendsAccountSearchStorage(this))
    , m_invites(new TFriendsInviteStorage(this))
{
}

TFriendsStorage* TFriendsStorage::instance()
{
    static TFriendsStorage* storage = new TFriendsStorage(QCoreApplication::instance());
    return storage;
}

void TFriendsStorage::setBadges(std::vector<TAccountBadge> badges)
{
    m_badges = std::move(badges);
    Q_EMIT accountBadgesChanged();
}

void TFriendsStorage::setSettings(std::vector<TVisibilitySetting> settings)
{
    m_settings = std::move(settings);
    Q_EMIT visibilitySettingsChanged();
}

TFriendsProtocolMessageHandler* TFriendsProtocolMessageHandler::instance()
{
    static TFriendsProtocolMessageHandler* handler = new TFriendsProtocolMessageHandler(QCoreApplication::instance());
    return handler;
}

void TFriendsProtocolMessageHandler::handleFriendSystemDataMessage(const GameserverMessageFriendSystemData& message)
{
    TFriendsStorage* storage = TFriendsStorage::instance();
    switch (static_cast<EFriendSystemData>(message.data_type())) {
    case EFriendSystemData::Error:
        // Every error is reported; code 2 is a failed account search.
        if (message.error_code() == 0)
            Q_EMIT cannotUseFriendSystemError();
        else if (message.error_code() == 1)
            Q_EMIT genericFriendSystemError(toQString(message.error_message()));
        else if (message.error_code() == 2)
            storage->accountSearch()->clear();
        Q_EMIT errorReceived();
        break;
    case EFriendSystemData::InviteNotice:
        Q_EMIT newFriendInviteReceived();
        break;
    case EFriendSystemData::FriendList: {
        std::vector<TFriendList::TFriendEntry> friendList;
        for (const auto& entry : message.friends()) {
            const auto& account = entry.account();
            TFriendList::TFriendEntry friendEntry;
            // A friend entry always names the account's main character.
            friendEntry.character = characterEntry(true, account.account_id(), account.character_id(), account.name(),
                                                   account.title(), account.world(), account.vocation(), account.level(),
                                                   account.outfit(), false);
            friendEntry.friendGroup = friendGroupFromProtocol(entry.friend_group());
            friendList.push_back(std::move(friendEntry));
        }
        storage->friendList()->setFriendList(friendList);
        break;
    }
    case EFriendSystemData::Invites: {
        TInviteList inviteList;
        for (const auto& invite : message.invites()) {
            TInviteList::TInviteListEntry entry;
            entry.isMainCharacter = true;
            entry.accountId = invite.account().account_id();
            entry.characterId = 0;
            entry.name = toQString(invite.account().name());
            entry.title = toQString(invite.account().title());
            entry.inviterName = toQString(invite.text());
            entry.timestamp = invite.timestamp();
            (invite.sent() ? inviteList.sent : inviteList.received).push_back(std::move(entry));
        }
        storage->inviteStorage()->setInviteList(std::move(inviteList));
        break;
    }
    case EFriendSystemData::Blacklist: {
        std::vector<TBlacklist::TBlacklistEntry> blacklist;
        for (const auto& entry : message.blacklist()) {
            TBlacklist::TBlacklistEntry blacklistEntry;
            blacklistEntry.isMainCharacter = true;
            blacklistEntry.accountId = entry.account().account_id();
            blacklistEntry.characterId = 0;
            blacklistEntry.name = toQString(entry.account().name());
            blacklistEntry.timestamp = entry.timestamp();
            blacklist.push_back(std::move(blacklistEntry));
        }
        storage->inviteStorage()->setBlacklist(std::move(blacklist));
        break;
    }
    case EFriendSystemData::AccountSearch: {
        const auto& search = message.account_search();
        TAccountSearchResult result;
        result.accountId = search.account_id();
        result.isFriendOrInvited = search.friend_or_invited();
        result.isOnBlacklist = search.on_blacklist();
        result.showAccountInfo = search.show_account_info();
        if (result.showAccountInfo) {
            result.isOnline = search.online();
            result.lastGameLogin = search.last_login();
            result.isPremium = search.premium();
            result.loyaltyTitle = toQString(search.loyalty_title());
            // Badges of a searched account carry no description and count as unlocked and shown.
            for (const auto& badge : search.badges())
                result.badges.emplace_back(badge.badge_id(), toQString(badge.name()), QString(), true, true);
        }
        for (const auto& character : search.characters()) {
            result.characters.push_back(characterEntry(character.main_character(), 0, character.character_id(), character.name(),
                                                       character.title(), character.world(), character.vocation(),
                                                       character.level(), character.outfit(), character.online()));
        }
        storage->accountSearch()->setResult(std::move(result));
        break;
    }
    case EFriendSystemData::Badges: {
        std::vector<TAccountBadge> badges;
        // The proto's "icon" string is the badge description.
        for (const auto& badge : message.badges())
            badges.emplace_back(badge.badge_id(), toQString(badge.name()), toQString(badge.icon()), badge.unlocked(), badge.visible());
        storage->setBadges(std::move(badges));
        break;
    }
    case EFriendSystemData::FriendshipNotice:
        Q_EMIT newFriendshipReceived();
        break;
    case EFriendSystemData::Settings: {
        std::vector<TVisibilitySetting> settings;
        for (const auto& setting : message.settings()) {
            settings.push_back({ friendGroupFromProtocol(setting.setting_type()), setting.show_character_info(),
                                 setting.show_account_info(), setting.allow_inspect() });
        }
        storage->setSettings(std::move(settings));
        break;
    }
    }
    // Product pages (IdleOT social) still listen on the notify bus.
    tibia::game::notify("onFriendSystemData", static_cast<int>(message.data_type()));
}

}
