#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QJsonObject>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <memory>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
}

namespace tibia {
namespace chat {

class TClientChatEntryFactory;

/// What a chat channel is (CIP: TChatChannelIdentifierBase and its subclasses
/// TLocalChatChannelIdentifier, TServerLogChannelIdentifier,
/// TNpcChatChannelIdentifier, TChannelIDChatChannelIdentifier,
/// THelpChannelIdentifier, TMessageToChatChannelIdentifier,
/// TOwnPrivateChannelToOpenIdentifier, TLootChannelIdentifier). One value type
/// stands in for the class family: `type` is what vslot 3 returns, `name` the
/// string every identifier carries, `id` the server channel's id.
struct TChatChannelIdentifier
{
    enum Type : int
    {
        Local = 0,
        ServerLog = 1,
        Npc = 2,
        /// A server channel, by id.
        ChannelId = 3,
        /// A private conversation; `name` is the partner.
        MessageTo = 4,
        OwnPrivateChannelToOpen = 5,
        Loot = 6,
    };

    Type type = Local;
    uint16_t id = 0;
    /// The server's channel name, or the private partner.
    QString name;

    static TChatChannelIdentifier channel(uint16_t id, const QString& name = {});
    static TChatChannelIdentifier messageTo(const QString& name);
    /// THelpChannelIdentifier: server channel 7 named after the help channel.
    static TChatChannelIdentifier help();

    /// vslot 2: the client names Local Chat, Server Log, NPCs and Loot itself;
    /// the others show the name they carry.
    QString displayName() const;
    /// The identifiers name the same channel: equal types, and for server
    /// channels the same id, for server, private and own channels the same
    /// name (case sensitive).
    bool sameChannel(const TChatChannelIdentifier& other) const;
    /// The kind of channel the identifier names.
    Type identifierType() const { return type; }
    /// A server channel whose id is in [first, last].
    bool isChannelIdInRange(uint16_t first, uint16_t last) const { return first <= id && id <= last; }
    /// Every channel but Local Chat and the Server Log can be closed.
    bool canBeClosed() const { return type != Local && type != ServerLog; }

    /// Server channel 7 (the help channel).
    bool isHelpChannel() const { return id == 7; }
    /// Server channels 5 and 6 (the advertising channels).
    bool isAdvertisingChannel() const { return isChannelIdInRange(5, 6); }
    /// Ids 10-9999 are the players' private channels.
    bool isPrivateChannelId() const { return isChannelIdInRange(10, 9999); }
    /// Ids 20000-29999 are Team Finder channels.
    bool isTeamFinderChannel() const { return isChannelIdInRange(20000, 29999); }
    /// Guild channels: ids 10000-19999 (Crystal's CHANNEL_GUILD is 0).
    bool isGuildChannel() const
    {
        return type == ChannelId && (id == 0 || (id >= 10000 && id < 20000));
    }
    /// Party channels: Crystal's CHANNEL_PARTY 1 and the Team Finder ids.
    bool isPartyChannel() const
    {
        return type == ChannelId && (id == 1 || (id >= 20000 && id < 30000));
    }
};

/// One line of a channel (CIP's chat entry, 0x60 bytes).
struct TChatEntry
{
    enum Kind : int
    {
        /// System text.
        System = 0,
        /// A speaker without level, e.g. an NPC (keywords in {} are links).
        Speaker = 1,
        /// A player with level.
        SpeakerWithLevel = 2,
        /// A loot message ({#rrggbb|text} coloured).
        Loot = 3,
    };

    Kind kind = System;
    QDateTime time;
    QString text;
    QString name;
    int level = 0;
    bool traded = false;
    QColor color;
    uint32_t statementId = 0;
};

/// A tutorial marker on a keyword of a channel entry (CIP:
/// TKeywordGuiTutorialMarkerInfo).
struct TKeywordGuiTutorialMarkerInfo
{
    int markerId = 0;
    QString keyword;
    /// The id of the entry the keyword is in.
    uint32_t entryId = 0;
};

/// One chat tab's content (CIP: chat::TChatChannel). Entries have ids: the
/// oldest kept one is firstEntryId, the next one added gets afterLastEntryId.
class TChatChannel final : public cip::tibia::chat::TChatChannel
{
    Q_OBJECT
public:
    TChatChannel(uint32_t maxEntries, const TChatChannelIdentifier& identifier, QObject* parent = nullptr);

    /// v12.
    const TChatChannelIdentifier& identifier() const { return m_identifier; }
    /// v13: a full channel drops its oldest entry first.
    void addEntry(const TChatEntry& entry);
    /// v14.
    void removeOldestEntry();
    /// v15: the ids go on from where they were.
    void clear();
    /// v16: the entry with id `id` (firstEntryId <= id < afterLastEntryId).
    const TChatEntry& entry(uint32_t id) const;
    /// v17.
    uint32_t entryCount() const { return static_cast<uint32_t>(m_entries.size()); }
    /// v18.
    uint32_t firstEntryId() const { return m_firstEntryId; }
    /// v19.
    uint32_t afterLastEntryId() const { return m_firstEntryId + entryCount(); }
    /// v20: the server closed the channel.
    bool isClosed() const { return m_isClosed; }
    /// v21.
    void setClosed(bool closed);
    /// v22: the tab becomes current when the channel opens or reopens.
    bool makeCurrentOnOpen() const { return m_makeCurrentOnOpen; }
    /// v23.
    void setMakeCurrentOnOpen(bool makeCurrent) { m_makeCurrentOnOpen = makeCurrent; }
    /// v24: the Server Log always shows it.
    bool isSubscribedToServerLog() const;
    /// v25.
    void setSubscribedToServerLog(bool subscribed);
    /// v26: the player owns this private channel (may invite and exclude).
    bool isOwnPrivateChannel() const { return m_isOwnPrivateChannel; }
    /// v27.
    void setOwnPrivateChannel(bool own) { m_isOwnPrivateChannel = own; }
    /// v28: the channel is shown in the secondary pane.
    bool isShownAsSecondaryChannel() const { return m_isShownAsSecondaryChannel; }
    /// v29.
    void setShownAsSecondaryChannel(bool shown) { m_isShownAsSecondaryChannel = shown; }
    /// v30.
    bool isMuted() const { return m_isMuted; }
    /// v31.
    void setMuted(bool muted) { m_isMuted = muted; }
    /// v32: marks the last entry holding "{keyword}" (case insensitive).
    /// True while the channel has a marker.
    bool addTutorialMarkerForKeyword(int markerId, const QString& keyword);
    /// v33.
    void removeTutorialMarker();
    /// v34.
    std::shared_ptr<TKeywordGuiTutorialMarkerInfo> tutorialMarker() const { return m_tutorialMarker; }

    QString name() const { return m_identifier.displayName(); }
    const std::deque<TChatEntry>& entries() const { return m_entries; }
    bool canBeClosed() const { return m_identifier.canBeClosed(); }
    /// The channels that can be closed can also show the server log.
    bool canSubscribeToServerLog() const { return canBeClosed(); }
    /// Typing in the channel speaks on the map (no NPC, server or private
    /// target).
    bool writesToLocalChat() const;

    /// The backlog length: setting CHAT/maxChatBacklogLength, default 1000,
    /// at least 5.
    static uint32_t maxEntries();

Q_SIGNALS:
    void idRangeChanged(uint32_t firstId, uint32_t afterLastId);
    void chatEntryAdded();
    // serverLogSubscriptionChanged / isClosedChanged / tutorialMarkerChanged
    // are inherited from cip::tibia::chat::TChatChannel.

private:
    /// A marker on an entry that left the channel goes too.
    void dropStaleTutorialMarker();

    uint32_t m_maxEntries;
    TChatChannelIdentifier m_identifier;
    std::deque<TChatEntry> m_entries;
    uint32_t m_firstEntryId = 0;
    bool m_isClosed = false;
    bool m_makeCurrentOnOpen = true;
    bool m_isSubscribedToServerLog = false;
    bool m_isOwnPrivateChannel = false;
    bool m_isShownAsSecondaryChannel = false;
    bool m_isMuted = false;
    std::shared_ptr<TKeywordGuiTutorialMarkerInfo> m_tutorialMarker;
};

/// The open channels and where each kind of entry goes (CIP:
/// chat::TChatChannelStorage, vtable slots 12-39). Local Chat and the Server
/// Log always exist.
class TChatChannelStorage final : public cip::tibia::chat::TChatChannelStorage
{
    Q_OBJECT
public:
    explicit TChatChannelStorage(std::shared_ptr<TClientChatEntryFactory> entryFactory, QObject* parent = nullptr);

    const std::vector<TChatChannel*>& channels() const { return m_channels; }

    /// v12: a system line in every channel that shows the server log.
    void addServerLogEntry(const QColor& color, const QString& text);
    /// v13: a loot line in the Loot channel, or without one in every channel
    /// that shows the server log.
    void addLootEntry(const QColor& color, const QString& text);
    /// v14: a system line in the reserved channel, if there is one.
    void addReservedChannelEntry(const QColor& color, const QString& text);
    /// v15: speech on the map; level 0 makes a speaker entry without level.
    void addLocalEntry(const QColor& color, const QString& name, const QString& text, int level, bool traded,
                       uint32_t statementId);
    /// v16: an NPC's line in the NPCs channel, opened in the background.
    void addNpcChannelEntry(const QColor& color, const QString& name, const QString& text);
    /// v17: the player's own line in the NPCs channel, opened in front.
    void addOwnNpcChannelEntry(const QColor& color, const QString& name, const QString& text, int level);
    /// v18: a message in an open server channel.
    void addChannelEntry(const QColor& color, uint16_t channelId, const QString& name, const QString& text, int level,
                         bool traded, uint32_t statementId);
    /// v19: a system line in an open server channel, or without it in every
    /// channel that shows the server log.
    void addChannelSystemEntry(const QColor& color, uint16_t channelId, const QString& text);
    /// v20: a system line in an open server channel only.
    void addChannelSystemEntryIfOpen(const QColor& color, uint16_t channelId, const QString& text);
    /// v21: a private message with `partner`, in its tab or else Local Chat.
    void addPrivateEntry(const QColor& color, const QString& partner, const QString& name, const QString& text, int level,
                         bool traded, uint32_t statementId);
    /// v22: a broadcast, in every channel that shows the server log.
    void addBroadcastEntry(const QColor& color, const QString& name, const QString& text, int level, bool traded,
                           uint32_t statementId);
    /// v23: the channel, opened if it is not (reopened if the server closed
    /// it). A new channel takes `makeCurrentOnOpen`; `forceMakeCurrent` goes
    /// to the tabs.
    TChatChannel* open(const TChatChannelIdentifier& identifier, bool makeCurrentOnOpen, bool forceMakeCurrent);
    /// v24: a channel the server opened, with its participants.
    void openServerChannel(uint16_t channelId, const QString& name, bool ownPrivateChannel, const QStringList& participants);
    /// v25.
    void openPrivateChannel(const QString& partner, bool makeCurrentOnOpen);
    /// v26: the server closed a channel.
    void closeChannelByServer(uint16_t channelId);
    /// v27.
    void clearAllChannels();
    /// v28.
    void removeChannel(const TChatChannelIdentifier& identifier);
    /// v29.
    size_t channelCount() const { return m_channels.size(); }
    /// v30.
    TChatChannel* find(const TChatChannelIdentifier& identifier) const;
    /// v31: the open (not closed) server channel with this id.
    TChatChannel* findChannelId(uint16_t channelId) const;
    /// v32.
    TChatChannel* localChat() const { return m_localChat; }
    /// v33.
    TChatChannel* serverLog() const { return m_serverLog; }
    /// v34.
    TChatChannel* lootChannel() const { return m_lootChannel; }
    /// v35.
    TChatChannel* reservedChannel() const { return m_reservedChannel; }
    /// v36: the player's own private channel, if open.
    TChatChannel* ownChannel() const;
    /// v37.
    std::vector<TChatChannelIdentifier> identifiers() const;
    /// v38: a tutorial marker on a keyword of the NPCs channel.
    void addTutorialMarkerToNpcChannel(int markerId, const QString& keyword);
    /// v39.
    void removeAllTutorialMarkers();

    /// A system line in one channel (closed channel notes, client messages).
    void addSystemEntry(TChatChannel* channel, const QColor& color, const QString& text);
    /// The NPC dialog's own line; prefer addOwnNpcChannelEntry.
    void addNpcEntry(const QColor& color, const QString& text, const QString& name);

    void setOwnName(const QString& name) { m_ownName = name.toLower(); }
    const QString& ownName() const { return m_ownName; }

public Q_SLOTS:
    void onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<::tibia::creatures::TCreature> pPlayer);

Q_SIGNALS:
    void newChatChannelOpened(const tibia::chat::TChatChannelIdentifier& identifier, bool forceMakeCurrent);
    void chatChannelReopend(const tibia::chat::TChatChannelIdentifier& identifier, bool forceMakeCurrent);
    void chatChannelRemoved(const tibia::chat::TChatChannelIdentifier& identifier);
    void entryAddedToChatChannel(const tibia::chat::TChatChannelIdentifier& identifier);
    /// Host views: a channel came (or came back) and whether its tab becomes current.
    void channelAdded(TChatChannel* channel, bool makeCurrent);
    void channelRemoved(TChatChannel* channel);

private:
    /// Opens Local Chat and the Server Log.
    void openDefaultChannels();
    bool isOwnName(const QString& name) const;

    uint32_t m_maxEntries;
    std::shared_ptr<TClientChatEntryFactory> m_entryFactory;
    TChatChannel* m_localChat = nullptr;
    TChatChannel* m_serverLog = nullptr;
    TChatChannel* m_lootChannel = nullptr;
    /// CipSoft's fourth fixed channel after Local Chat, Server Log and Loot;
    /// nothing in this build of CipSoft's client assigns it, so it stays empty.
    TChatChannel* m_reservedChannel = nullptr;
    std::vector<TChatChannel*> m_channels;
    /// Lower case.
    QString m_ownName;

    /// The game action that plays a chat sound (CIP SOUND_TRIGGER) for a message of `channel`.
    void publishChatSound(int sound, const TChatChannelIdentifier& channel);
};

/// CIP's chat message mode of a protocol mode (the same numbers; 41 and
/// 45..47 and anything above 54 are invalid and throw).
int chatMessageMode(uint32_t protocolMode);
/// The protocol's mode of a chat message mode, for client messages (the same
/// numbers; the modes chatMessageMode refuses throw).
uint32_t protocolMessageMode(int chatMode);
/// CIP's console colour of a chat message mode (throws for a mode without one).
QColor consoleColor(int mode);
/// CIP's colour of a mode's text over the game window (throws for a mode
/// without one).
QColor overlayColor(int mode);
/// The system colours.
QColor systemRed();
QColor systemWhite();

/// CIP's chat line: "[hh:mm[:ss] ]name[ (Traded)][ [level]]: text"
/// as rich text in the entry's colour with a hanging indent.
QString formatEntry(const TChatEntry& entry, bool timestamps, bool seconds, bool levels, int* plainLength = nullptr);

/// Loot / look rich text: {id|name} (item-value colours when colorizeLootStyle ≠ 0)
/// then {#rrggbb|text}. Shared by console + map bubbles.
QString formatLootRichText(QString text);
/// "{#rrggbb|text}" -> "<font color="#rrggbb">text</font>".
QString replaceColorCodingsWithHtmlTags(const QString& textWithColorCodings);

}
}

Q_DECLARE_METATYPE(tibia::chat::TChatChannelIdentifier)
