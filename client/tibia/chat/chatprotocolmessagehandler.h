#pragma once

#include "client/gamecore.h"
#include <client/position.h>

#include <QColor>

#include <cip_gen.h>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
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
namespace worldmap {
class TNumericalEffectStorage;
}
namespace chat {

class TChatChannelStorage;
class TTextStorage;

/// The ignore and white lists (CIP: chat::TWhiteBlacklist, vtable slots
/// 12-36; option section whiteBlacklistOptions). Both lists are kept sorted
/// by lower-case name; only permanent entries are stored. The flags are not
/// stored: blacklist and whitelist start active, the rest off.
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

    /// v12: where allowVips looks names up.
    void setVipQuery(std::function<bool(const QString&)> isVip) { m_isVip = std::move(isVip); }
    /// v13: a name already listed is replaced.
    void addToBlacklist(const QString& name, bool permanently);
    /// v14.
    void removeFromBlacklist(const QString& name);
    /// v15.
    const Entries& blacklist() const { return m_blacklist; }
    /// v16.
    void addToWhitelist(const QString& name, bool permanently);
    /// v17.
    const Entries& whitelist() const { return m_whitelist; }
    /// v18.
    void setLists(const Entries& whitelist, const Entries& blacklist);
    /// v19: the stored permanent names.
    void loadFromOptions();
    /// v20.
    void saveToOptions() const;
    /// v21: a message from `name` is dropped (messageFiltered).
    bool isBlacklisted(const QString& name);
    /// v22: a yell from `name` is dropped (messageFiltered).
    bool filtersYell(const QString& name);
    /// v23: a private message from `name` is dropped (messageFiltered).
    bool filtersPrivateMessage(const QString& name);
    /// v24.
    bool isOnBlacklist(const QString& name) const;
    /// v25: the whitelist is active and has the name, or allows VIPs and
    /// the name is one.
    bool isWhitelisted(const QString& name) const;
    /// v26: message modes no list filters.
    static bool isExemptMode(int mode);
    /// v27 / v28.
    bool blacklistActive() const { return m_blacklistActive; }
    void setBlacklistActive(bool active) { m_blacklistActive = active; }
    /// v29 / v30.
    bool whitelistActive() const { return m_whitelistActive; }
    void setWhitelistActive(bool active) { m_whitelistActive = active; }
    /// v31 / v32.
    bool ignoreYelling() const { return m_ignoreYelling; }
    void setIgnoreYelling(bool ignore) { m_ignoreYelling = ignore; }
    /// v33 / v34.
    bool ignorePrivateMessages() const { return m_ignorePrivateMessages; }
    void setIgnorePrivateMessages(bool ignore) { m_ignorePrivateMessages = ignore; }
    /// v35 / v36.
    bool allowVips() const { return m_allowVips; }
    void setAllowVips(bool allow) { m_allowVips = allow; }

    /// The map's speech applies the same rules as the console, which reports
    /// the filtered message; these do not signal.
    bool blocksName(const QString& name) const;
    bool blocksYell(const QString& name) const;
    bool blocksPrivateMessage(const QString& name) const;

    bool isIgnored(const QString& name) const { return isOnBlacklist(name); }
    /// IGNORE_NAME / UNIGNORE_NAME.
    void ignore(const QString& name) { addToBlacklist(name, false); }
    void unignore(const QString& name) { removeFromBlacklist(name); }
    void setFlags(bool blacklistActive, bool whitelistActive, bool ignoreYelling, bool ignorePrivateMessages, bool allowVips);

private:
    static Entries::iterator findEntry(Entries& entries, const QString& name);
    static Entries::const_iterator findEntry(const Entries& entries, const QString& name);
    /// Replaces an entry of the same name, else appends.
    static void insertEntry(Entries& entries, const Entry& entry);
    static void sortEntries(Entries& entries);
    bool isVip(const QString& name) const;

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
/// Ingress: the protobuf messages reach onServerMessageEnqueued, which applies
/// them through the game session; the console listens on its notifies.
class TChatProtocolMessageHandler final : public cip::tibia::chat::TChatProtocolMessageHandler
{
    Q_OBJECT
public:
    TChatProtocolMessageHandler(TChatChannelStorage& storage, TWhiteBlacklist& blacklist, QObject* parent = nullptr);
    ~TChatProtocolMessageHandler() override;

    /// : the console options by CIP name.
    void setOptions(const QJsonObject& options);
    /// CIP +0x20: the game window's texts (the active session's).
    void setTextStorage(TTextStorage* textStorage) { m_textStorage = textStorage; }
    /// CIP +0x10: the game window's numerical effects (the active session's).
    void setNumericalEffectStorage(worldmap::TNumericalEffectStorage* effects) { m_numericalEffects = effects; }
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

    /// Tip Q→H wire: peeled chat opcodes from `serverMessageEnqueued`. Static —
    /// the console handler is owned by the chat controller, while the map
    /// (speech bubbles, numerical effects) and packs consume the same messages.
    static void onServerMessageEnqueued(int opcode);

public Q_SLOTS:
    /// CIP moc slot (cip_gen virtual) — reload console / UI-Sounds chat options.
    void onChatProtocolMessageHandlerOptionsChanged() override;
    /// CIP moc slot (shared_ptr skipped from cip_gen) — own-name for filter/sound.
    void onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<::tibia::creatures::TCreature> pPlayer);

Q_SIGNALS:
    /// Tip open-channel dialog list (classic notify path).
    void channelListReceived(const std::vector<std::pair<uint16_t, QString>>& channels);
    /// A private tab opened for an incoming message.
    void privateChannelOpened(const QString& name);
    /// CIP moc signal (TChatChannelIdentifierList → qtclient channel pairs).
    void currentlyAvailableChannels(const std::vector<std::pair<uint16_t, QString>>& ChannelList);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); qtclient name parity.
    void publishGameAction();

private:
    /// Local Chat: yells upper case (CIP volume 2).
    void addLocalChatEntry(const QString& name, int level, bool traded, uint32_t statementId, int volume, const QColor& color,
                           const QString& text);
    /// A private message; opens its tab first when new private messages
    /// open one.
    void addPrivateChatEntry(const QString& name, int level, bool traded, uint32_t statementId, const QString& text,
                             const QColor& color);
    /// The speech over the creature, when the game window shows messages.
    void showCreatureSpeech(const QString& name, int mode, const QString& text, const Position& position);
    /// The loot message on the map and in the loot or server log channel.
    void handleLootMessageForChannelAndStatusMessage(const QString& text);
    /// Whether the console options hide this mode's server log line.
    bool consoleFilters(int mode) const;
    /// .
    bool isOwnName(const QString& name) const;

    TChatChannelStorage& m_storage;
    TTextStorage* m_textStorage = nullptr;
    worldmap::TNumericalEffectStorage* m_numericalEffects = nullptr;
    TWhiteBlacklist& m_blacklist;
    std::vector<client::gamecore::Hook> m_hooks;
    /// The console handler the session's channel messages go to.
    static TChatProtocolMessageHandler* s_console;

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
    /// The game window options (CIP +0x80, +0x83..+0x8a).
    bool m_showTextualEffects = true;
    bool m_showMessages = true;
    bool m_showPrivateMessages = true;
    bool m_showBoostedCreatureMessages = true;
    bool m_showOfflineTrainingMessages = true;
    bool m_showStoreMessages = true;
    bool m_showPotionMessages = true;
    bool m_showHotkeyUsageMessages = true;
    bool m_showLootMessages = true;
    /// CIP +0x94: the colorizeLootStyle option (nonzero → loot lines in white).
    int m_colorizeLootStyle = 0;
};

}
}
