#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace protobuf {
namespace protocol {
class GameserverMessageChannelEvent;
class GameserverMessageChannels;
class GameserverMessageCloseChannel;
class GameserverMessageMessage;
class GameserverMessageNpcTalkParters;
class GameserverMessageOpenChannel;
class GameserverMessageOpenOwnChannel;
class GameserverMessagePrivateChannel;
class GameserverMessageTalk;
}
}
}

namespace tibia {
namespace chat {

class TChatChannelStorage;

/// The ignore and white lists (CIP: TWhiteBlacklist, ctor ,
/// vtable slots 12-36 at ..; option section
/// whiteBlacklistOptions). Both lists are kept sorted; only permanent entries
/// are stored . The flags are not stored: blacklist and
/// whitelist start active, the rest off (ctor +0x60 = 0x101).
class TWhiteBlacklist final : public cip::tibia::chat::TWhiteBlacklist
{
    Q_OBJECT
public:
    struct Entry
    {
        QString name;
        bool permanently = false;
    };
    using Entries = std::vector<Entry>;

    explicit TWhiteBlacklist(QObject* parent = nullptr);

    /// Slot 24 : the name is on the blacklist.
    bool isOnBlacklist(const QString& name) const;
    bool isIgnored(const QString& name) const { return isOnBlacklist(name); }
    /// Slot 25 : the whitelist is active and has the name, or
    /// allows VIPs and the name is one.
    bool isWhitelisted(const QString& name) const;
    /// Slot 21 .
    bool isBlacklisted(const QString& name) const;
    /// Slots 22 / 23 : every yell / private message
    /// but the whitelisted ones.
    bool filtersYell(const QString& name) const;
    bool filtersPrivateMessage(const QString& name) const;
    /// Slot 26 : message modes no list filters (mask 0x2000300000ec00).
    static bool isExemptMode(int mode);

    /// IGNORE_NAME / UNIGNORE_NAME (CIP): a temporary entry.
    void ignore(const QString& name);
    void unignore(const QString& name);

    const Entries& blacklist() const { return m_blacklist; }
    const Entries& whitelist() const { return m_whitelist; }
    /// Slot 18 .
    void setLists(const Entries& whitelist, const Entries& blacklist);

    bool blacklistActive() const { return m_blacklistActive; }
    bool whitelistActive() const { return m_whitelistActive; }
    bool ignoreYelling() const { return m_ignoreYelling; }
    bool ignorePrivateMessages() const { return m_ignorePrivateMessages; }
    bool allowVips() const { return m_allowVips; }
    void setFlags(bool blacklistActive, bool whitelistActive, bool ignoreYelling, bool ignorePrivateMessages, bool allowVips);
    /// : whether a name is on the VIP list.
    void setVipQuery(std::function<bool(const QString&)> isVip) { m_isVip = std::move(isVip); }
    /// CIP moc messageFiltered (inherited) — protocol path flashes the ignore button.
    void notifyMessageFiltered() { Q_EMIT messageFiltered(); }

Q_SIGNALS:
    void changed();
    // CIP moc messageFiltered is inherited from cip::tibia::chat::TWhiteBlacklist.

private:
    static bool contains(const Entries& entries, const QString& name);
    static void insert(Entries& entries, const Entry& entry);
    /// vslot 0xa0 .
    void save();

    Entries m_blacklist;
    Entries m_whitelist;
    bool m_blacklistActive = true;
    bool m_whitelistActive = true;
    bool m_ignoreYelling = false;
    bool m_ignorePrivateMessages = false;
    bool m_allowVips = false;
    std::function<bool(const QString&)> m_isVip;
};

/// The console side of the server's chat messages (CIP:
/// TChatProtocolMessageHandler, vtable): talk ,
/// text messages , channels (open, 
/// own channel, close, list, private,
/// events). The map side is TGameMessageHandler.
///
/// Live ingress today is classic `kNotifyScope` / TGameNotify (CIP queue →
/// handle*Message slots land once protobuf fields are recovered).
class TChatProtocolMessageHandler final : public cip::tibia::chat::TChatProtocolMessageHandler
{
    Q_OBJECT
public:
    TChatProtocolMessageHandler(TChatChannelStorage& storage, TWhiteBlacklist& blacklist, QObject* parent = nullptr);

    /// : the console options by CIP name.
    void setOptions(const QJsonObject& options);
    /// The server's next GameServerOpenChannel for `channelId` makes the tab
    /// current (OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT). Channels the server opens
    /// by itself do not ('s last argument is 0 from).
    void openInForeground(uint16_t channelId) { m_foregroundChannels.insert(channelId); }
    /// openInForeground's id for the player's own private channel (GameServerOpenOwnChannel).
    static constexpr uint16_t OwnChannelRequest = 0xFFFF;

    /// CIP slots (protobuf args; empty layouts → classic notify remains live apply).
    void handleTalkMessage(const ::tibia::protobuf::protocol::GameserverMessageTalk& TalkMessage);
    void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& MessageMessage);
    void handleOpenChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenChannel& OpenChannelMessage);
    void handleOpenOwnChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenOwnChannel& OpenChannelMessage);
    void handleCloseChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseChannel& CloseChannelMessage);
    void handleChannelsMessage(const ::tibia::protobuf::protocol::GameserverMessageChannels& ChannelsMessage);
    void handlePrivateChannelMessage(const ::tibia::protobuf::protocol::GameserverMessagePrivateChannel& PrivateChannel);
    void handleChannelEventMessage(const ::tibia::protobuf::protocol::GameserverMessageChannelEvent& ChannelEvent);
    void handleNpcTalkPartersMessage(const ::tibia::protobuf::protocol::GameserverMessageNpcTalkParters& Message);

public Q_SLOTS:
    /// CIP moc slot (cip_gen virtual) — reload console / UI-Sounds chat options.
    void onChatProtocolMessageHandlerOptionsChanged() override;
    /// CIP moc slot (shared_ptr skipped from cip_gen) — own-name for filter/sound.
    void onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<::tibia::creatures::TCreature> pPlayer);

Q_SIGNALS:
    /// Tip open-channel dialog list (classic notify path).
    void channelListReceived(const std::vector<std::pair<uint16_t, QString>>& channels);
    /// Tip ignore-button flash (also CIP TWhiteBlacklist::messageFiltered).
    void messageFiltered();
    /// A private tab opened for an incoming message.
    void privateChannelOpened(const QString& name);
    /// CIP moc signal (TChatChannelIdentifierList → tip channel pairs).
    void currentlyAvailableChannels(const std::vector<std::pair<uint16_t, QString>>& ChannelList);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); tip name parity.
    void publishGameAction();

private:
    void onTalk(const QString& name, int level, int mode, const QString& text, uint16_t channelId, uint32_t statementId);
    void onTextMessage(int mode, const QString& text);
    void onChannelTextMessage(int mode, uint16_t channelId, const QString& text);
    /// : joined (0) / left (1) lines in the channel, behind consoleShowJoinLeaveMessages;
    /// invite / exclude events add nothing.
    void onChannelEvent(uint16_t channelId, const QString& name, int type);
    void onCloseChannel(uint16_t channelId);
    /// .
    void addPrivateMessage(const QString& name, int level, int mode, const QString& text, uint32_t statementId);
    bool isOwnName(const QString& name) const;
    /// Whether UI-Sounds chat options allow a CHAT_MESSAGE_ARRIVED for this mode/channel.
    bool chatArrivedSoundAllowed(int mode, uint16_t channelId) const;
    /// Whether a console line for this mode/channel may be shown (same *MessagesEnabled keys).
    bool consoleMessageAllowed(int mode, uint16_t channelId) const;

    TChatChannelStorage& m_storage;
    TWhiteBlacklist& m_blacklist;
    std::vector<client::gamecore::Hook> m_hooks;
    std::unordered_set<uint16_t> m_foregroundChannels;

    /// Prefaults match (ids 0x1c/0x1d/0x20/0x1f/0x1b/0x1a/0x46/0x45)
    /// until setOptions runs.
    bool m_showEventMessages = true;
    bool m_showInfoMessages = true;
    bool m_showStatusMessagesOwn = true;
    bool m_showStatusMessagesOfOthers = true;
    bool m_showJoinLeaveMessages = true;
    bool m_openPrivateMessagesInNewTab = false;
    bool m_showOwnSpells = true;
    bool m_showOthersSpells = true;
    /// UI Sounds → chat subcategory (CIP OptionsPageUISounds / option ids 0xad–0xba).
    bool m_chatSoundsEnabled = true;
    bool m_partyMessagesEnabled = true;
    bool m_guildMessagesEnabled = true;
    bool m_privateMessagesEnabled = true;
    bool m_npcMessagesEnabled = true;
    bool m_globalMessagesEnabled = true;
    bool m_teamFinderMessagesEnabled = true;
    bool m_raidMessagesEnabled = true;
    bool m_systemMessagesEnabled = true;
};

}
}
