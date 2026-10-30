#pragma once

#include "client/gamecore.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tibia::chat {

class TChatChannelStorage;

/// The ignore and white lists (CIP: TWhiteBlacklist, ctor 0x1009a5a30,
/// vtable 0x102623d28 slots 12-36 at 0x1009a5af0..0x1009a6f80; option section
/// whiteBlacklistOptions). Both lists are kept sorted; only permanent entries
/// are stored (0x1009a6690). The flags are not stored: blacklist and
/// whitelist start active, the rest off (ctor +0x60 = 0x101).
class TWhiteBlacklist final : public QObject
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

    /// Slot 24 (0x1009a6b70): the name is on the blacklist.
    bool isOnBlacklist(const QString& name) const;
    bool isIgnored(const QString& name) const { return isOnBlacklist(name); }
    /// Slot 25 (0x1009a6b90): the whitelist is active and has the name, or
    /// allows VIPs and the name is one.
    bool isWhitelisted(const QString& name) const;
    /// Slot 21 (0x1009a6a70).
    bool isBlacklisted(const QString& name) const;
    /// Slots 22 / 23 (0x1009a6af0 / 0x1009a6b30): every yell / private message
    /// but the whitelisted ones.
    bool filtersYell(const QString& name) const;
    bool filtersPrivateMessage(const QString& name) const;
    /// Slot 26 (0x1009a6d80): message modes no list filters (mask 0x2000300000ec00).
    static bool isExemptMode(int mode);

    /// IGNORE_NAME / UNIGNORE_NAME (CIP 0x1009933d0 / 0x100993670): a temporary entry.
    void ignore(const QString& name);
    void unignore(const QString& name);

    const Entries& blacklist() const { return m_blacklist; }
    const Entries& whitelist() const { return m_whitelist; }
    /// Slot 18 (0x1009a61c0).
    void setLists(const Entries& whitelist, const Entries& blacklist);

    bool blacklistActive() const { return m_blacklistActive; }
    bool whitelistActive() const { return m_whitelistActive; }
    bool ignoreYelling() const { return m_ignoreYelling; }
    bool ignorePrivateMessages() const { return m_ignorePrivateMessages; }
    bool allowVips() const { return m_allowVips; }
    void setFlags(bool blacklistActive, bool whitelistActive, bool ignoreYelling, bool ignorePrivateMessages, bool allowVips);
    /// FUN_1009a6be0: whether a name is on the VIP list.
    void setVipQuery(std::function<bool(const QString&)> isVip) { m_isVip = std::move(isVip); }

Q_SIGNALS:
    void changed();

private:
    static bool contains(const Entries& entries, const QString& name);
    static void insert(Entries& entries, const Entry& entry);
    /// vslot 0xa0 (0x1009a6690).
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
/// TChatProtocolMessageHandler, vtable 0x1007e50d0): talk (FUN_100996120),
/// text messages (FUN_100997530), channels (FUN_1009983c0 open, FUN_1009985d0
/// own channel, FUN_1009987e0 close, FUN_100998810 list, FUN_100998db0 private,
/// FUN_100998e90 events). The map side is TGameMessageHandler.
class TChatProtocolMessageHandler final : public QObject
{
    Q_OBJECT
public:
    TChatProtocolMessageHandler(TChatChannelStorage& storage, TWhiteBlacklist& blacklist, QObject* parent = nullptr);

    /// FUN_100995c70: the console options by CIP name.
    void setOptions(const QJsonObject& options);
    /// The server's next GameServerOpenChannel for `channelId` makes the tab
    /// current (OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT). Channels the server opens
    /// by itself do not (0x10098f070's last argument is 0 from 0x1009983c0).
    void openInForeground(uint16_t channelId) { m_foregroundChannels.insert(channelId); }
    /// openInForeground's id for the player's own private channel (GameServerOpenOwnChannel).
    static constexpr uint16_t OwnChannelRequest = 0xFFFF;

Q_SIGNALS:
    /// The server's channel list (GameServerChannels), for the open dialog.
    void channelListReceived(const std::vector<std::pair<uint16_t, QString>>& channels);
    /// A message was dropped because its sender is ignored (the ignore button flashes).
    void messageFiltered();
    /// A private tab opened for an incoming message.
    void privateChannelOpened(const QString& name);

private:
    void onTalk(const QString& name, int level, int mode, const QString& text, uint16_t channelId, uint32_t statementId);
    void onTextMessage(int mode, const QString& text);
    void onChannelTextMessage(int mode, uint16_t channelId, const QString& text);
    /// FUN_100998e90: joined (0) / left (1) lines in the channel, behind consoleShowJoinLeaveMessages;
    /// invite / exclude events add nothing.
    void onChannelEvent(uint16_t channelId, const QString& name, int type);
    void onCloseChannel(uint16_t channelId);
    /// FUN_100996d10.
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

    /// Prefaults match FUN_1009ad850 (ids 0x1c/0x1d/0x20/0x1f/0x1b/0x1a/0x46/0x45)
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
