#pragma once

#include "shared/cip_typed.h"

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <memory>
#include <vector>

namespace tibia::chat {

/// What a chat channel is (CIP: TChatChannelIdentifierBase and its subclasses;
/// vslot 3 is the type, vslot 2 the tab name).
struct TChatChannelIdentifier
{
    enum Type : int
    {
        Local = 0,
        ServerLog = 1,
        Npc = 2,
        /// A server channel (TChannelIDChatChannelIdentifier, id at +0x20).
        ChannelId = 3,
        /// A private conversation (TMessageToChatChannelIdentifier, name at +8).
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
    /// CIP THelpChannelIdentifier — server channel 7 (FUN_10098ce10).
    static TChatChannelIdentifier help();

    /// vslot 2: CIP names its own channels (qtTrId), the server names the others.
    QString displayName() const;
    bool sameChannel(const TChatChannelIdentifier& other) const;
    /// FUN_10098c6c0: CIP guild ids 10000-19999. Crystal CHANNEL_GUILD is 0.
    bool isGuildChannel() const
    {
        return type == ChannelId && (id == 0 || (id >= 10000 && id < 20000));
    }
    /// FUN_10098c6e0: CIP party ids 20000-29999. Crystal CHANNEL_PARTY is 1.
    bool isPartyChannel() const
    {
        return type == ChannelId && (id == 1 || (id >= 20000 && id < 30000));
    }
};

/// CIP polymorphic channel identity (vslot 3 = type). Live value type is
/// TChatChannelIdentifier; these named types are constructed at factory sites.
class TChatChannelIdentifierBase : public ::tibia::CipTyped
{
public:
    explicit TChatChannelIdentifierBase(TChatChannelIdentifier::Type type);
    TChatChannelIdentifier::Type type() const { return m_type; }

protected:
    TChatChannelIdentifier::Type m_type;
};

class TLocalChatChannelIdentifier final : public TChatChannelIdentifierBase
{
public:
    TLocalChatChannelIdentifier();
};

class TServerLogChannelIdentifier final : public TChatChannelIdentifierBase
{
public:
    TServerLogChannelIdentifier();
};

class TNpcChatChannelIdentifier final : public TChatChannelIdentifierBase
{
public:
    TNpcChatChannelIdentifier();
};

class TChannelIDChatChannelIdentifier : public TChatChannelIdentifierBase
{
public:
    explicit TChannelIDChatChannelIdentifier(uint16_t id, QString name = {});
    uint16_t id() const { return m_id; }
    const QString& name() const { return m_name; }

private:
    uint16_t m_id = 0;
    QString m_name;
};

class TMessageToChatChannelIdentifier final : public TChatChannelIdentifierBase
{
public:
    explicit TMessageToChatChannelIdentifier(QString partner);
    const QString& partner() const { return m_partner; }

private:
    QString m_partner;
};

class TOwnPrivateChannelToOpenIdentifier final : public TChatChannelIdentifierBase
{
public:
    TOwnPrivateChannelToOpenIdentifier();
};

class TLootChannelIdentifier final : public TChatChannelIdentifierBase
{
public:
    TLootChannelIdentifier();
};

/// CIP THelpChannelIdentifier — ChannelId 7 with help tab name (qtTrId 0x2405fad).
class THelpChannelIdentifier final : public TChannelIDChatChannelIdentifier
{
public:
    static constexpr uint16_t kChannelId = 7;
    THelpChannelIdentifier();
};

/// One line of a channel (CIP: the entry FUN_1009915d0 / FUN_100991640 /
/// FUN_1009916d0 / FUN_100991560 build).
struct TChatEntry
{
    enum Kind : int
    {
        /// System text (FUN_1009915d0).
        System = 0,
        /// A speaker without level, e.g. an NPC (FUN_100991640; keywords in {} are links).
        Speaker = 1,
        /// A player with level (FUN_1009916d0).
        SpeakerWithLevel = 2,
        /// A loot message ({#rrggbb|text} coloured, FUN_100991560).
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

/// One chat tab's content (CIP: TChatChannel, vtable 0x1007e39a0).
class TChatChannel final : public QObject
{
    Q_OBJECT
public:
    explicit TChatChannel(const TChatChannelIdentifier& identifier, QObject* parent = nullptr);

    const TChatChannelIdentifier& identifier() const { return m_identifier; }
    QString name() const { return m_identifier.displayName(); }
    const std::deque<TChatEntry>& entries() const { return m_entries; }

    void addEntry(TChatEntry entry);
    void clear();

    /// FUN_10099b780: every channel but Local Chat and the Server Log.
    bool canBeClosed() const;
    /// FUN_10099b7c0: the same channels can show the server log.
    bool canSubscribeToServerLog() const { return canBeClosed(); }
    /// The Server Log always shows it.
    bool isSubscribedToServerLog() const { return m_identifier.type == TChatChannelIdentifier::ServerLog || m_subscribedToServerLog; }
    void setSubscribedToServerLog(bool subscribed);
    /// Typing in the channel speaks on the map (CIP's sendChatMessage: no NPC,
    /// server or private target).
    bool writesToLocalChat() const;

    /// The channel the player owns (onOpenOwnPrivateChannel): invite and exclude.
    bool isOwnChannel() const { return m_ownChannel; }
    void setOwnChannel(bool own) { m_ownChannel = own; }
    /// The server closed it (FUN_10098fa10 flag 0xa8).
    bool isClosedByServer() const { return m_closedByServer; }
    void setClosedByServer(bool closed) { m_closedByServer = closed; }
    /// vslot 0xe0: a muted channel's tab does not flash (0x100bd8b30).
    bool isMuted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }

    /// Oldest entries are dropped beyond this many: setting
    /// CHAT/maxChatBacklogLength, default 1000, at least 5 (CIP 0x1009ab0f0).
    static size_t maxEntries();

Q_SIGNALS:
    /// The entry appended last (oldest first in entries()).
    void entryAdded();
    /// The oldest entry left (maxEntries()).
    void oldestEntryRemoved();
    void cleared();
    void propertiesChanged();

private:
    TChatChannelIdentifier m_identifier;
    std::deque<TChatEntry> m_entries;
    bool m_subscribedToServerLog = false;
    bool m_ownChannel = false;
    bool m_closedByServer = false;
    bool m_muted = false;
};

/// The open channels in tab order and where each kind of entry goes (CIP:
/// TChatChannelStorage, vtable slots 12-39 at 0x10098daf0..0x100990950).
/// Local Chat and the Server Log always exist.
class TChatChannelStorage final : public QObject
{
    Q_OBJECT
public:
    explicit TChatChannelStorage(QObject* parent = nullptr);

    const std::vector<TChatChannel*>& channels() const { return m_channels; }
    TChatChannel* localChat() const { return m_channels.front(); }
    /// vslot 30 (0x100990010).
    TChatChannel* find(const TChatChannelIdentifier& identifier) const;
    TChatChannel* findChannelId(uint16_t id) const { return find(TChatChannelIdentifier::channel(id)); }
    /// The private channel the player owns, if open.
    TChatChannel* ownChannel() const;
    /// vslot 23 (0x10098f070): the channel, opened (appended) if it is not.
    TChatChannel* open(const TChatChannelIdentifier& identifier, bool makeCurrent);
    void close(TChatChannel* channel);
    /// Back to Local Chat and the Server Log, both empty.
    void reset();
    void moveChannel(int from, int to);

    /// The player's name: own channel and private messages make no "new
    /// entry" notification.
    void setOwnName(const QString& name) { m_ownName = name; }
    const QString& ownName() const { return m_ownName; }

    /// vslot 12 (0x10098daf0): to every channel that shows the server log.
    void addServerLogEntry(const QColor& color, const QString& text);
    /// vslot 15 (0x10098de50): speech on the map; level 0 makes a Speaker entry.
    void addLocalEntry(const QColor& color, const QString& text, const QString& name, int level, uint32_t statementId);
    /// vslot 16 (0x10098df40): the NPCs channel, opened if needed.
    void addNpcEntry(const QColor& color, const QString& text, const QString& name);
    /// vslot 18 (0x10098e560): a message in an open server channel.
    void addChannelEntry(uint16_t channelId, const QColor& color, const QString& text, const QString& name, int level, uint32_t statementId);
    /// vslot 19 (0x10098e900): channel management text (joins, invitations).
    void addChannelSystemEntry(uint16_t channelId, const QColor& color, const QString& text);
    /// vslot 21 (0x10098eb80): a private message with `partner`, its tab opened if needed.
    void addPrivateEntry(const QString& partner, const QColor& color, const QString& text, const QString& name, int level, uint32_t statementId);
    /// vslot 22 (0x10098ef80): a gamemaster broadcast, to every channel showing the server log.
    void addBroadcastEntry(const QColor& color, const QString& text, const QString& name, int level, uint32_t statementId);
    /// The Loot channel, if open (FUN_100997ed0).
    void addLootEntry(const QColor& color, const QString& text);
    /// A system line in one channel (ADD_SYSTEM_CHAT_MESSAGE, closed channel notes).
    void addSystemEntry(TChatChannel* channel, const QColor& color, const QString& text);

Q_SIGNALS:
    void channelAdded(TChatChannel* channel, bool makeCurrent);
    void channelRemoved(TChatChannel* channel);
    void channelsMoved();
    /// reset() replaced every channel.
    void channelsReset();
    /// A channel got an entry someone else wrote (the tab flashes).
    void entryAddedToChannel(TChatChannel* channel, bool fromOthers);

private:
    TChatChannel* create(const TChatChannelIdentifier& identifier);
    void add(TChatChannel* channel, TChatEntry entry);

    std::vector<TChatChannel*> m_channels;
    QString m_ownName;
};

/// CIP's console colour of a protocol message mode (FUN_1009911c0, table
/// DAT_1022bc610 / DAT_1022bc7b8).
QColor consoleColor(int mode);
/// The system colours (DAT_1022bc200 red, DAT_1022bc210 white).
QColor systemRed();
QColor systemWhite();

/// CIP's chat line (FUN_100bd75e0): "[hh:mm[:ss] ]name[ (Traded)][ [level]]: text"
/// as rich text in the entry's colour with a hanging indent.
QString formatEntry(const TChatEntry& entry, bool timestamps, bool seconds, bool levels);

/// Loot / look rich text: {id|name} (item-value colours when colorizeLootStyle ≠ 0)
/// then {#rrggbb|text} (CIP FUN_10099c150 + FUN_10099c870). Shared by console + map bubbles.
QString formatLootRichText(QString text);

/// CIP's clamped integer setting (0x1009a9fb0): the QSettings value in the
/// group, a value below the minimum is stored back as the minimum.
int chatSetting(const QString& key, int defaultValue, int minimum);

}
