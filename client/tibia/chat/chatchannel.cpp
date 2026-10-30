#include "chatchannel.h"

#include "chat/chatgameactionhandler.h"
#include "cyclopedia/iteminfostorage.h"

#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace tibia::chat {

namespace {

QDateTime now()
{
    return QDateTime::currentDateTime();
}

} // namespace

TChatChannelIdentifier TChatChannelIdentifier::channel(const uint16_t id, const QString& name)
{
    (void)TChannelIDChatChannelIdentifier(id, name);
    return { ChannelId, id, name };
}

TChatChannelIdentifier TChatChannelIdentifier::messageTo(const QString& name)
{
    (void)TMessageToChatChannelIdentifier(name);
    return { MessageTo, 0, name };
}

TChatChannelIdentifier TChatChannelIdentifier::help()
{
    (void)THelpChannelIdentifier();
    return channel(THelpChannelIdentifier::kChannelId);
}

TChatChannelIdentifierBase::TChatChannelIdentifierBase(const TChatChannelIdentifier::Type type)
    : m_type(type)
{
}

TLocalChatChannelIdentifier::TLocalChatChannelIdentifier()
    : TChatChannelIdentifierBase(TChatChannelIdentifier::Local)
{
}

TServerLogChannelIdentifier::TServerLogChannelIdentifier()
    : TChatChannelIdentifierBase(TChatChannelIdentifier::ServerLog)
{
}

TNpcChatChannelIdentifier::TNpcChatChannelIdentifier()
    : TChatChannelIdentifierBase(TChatChannelIdentifier::Npc)
{
}

TChannelIDChatChannelIdentifier::TChannelIDChatChannelIdentifier(const uint16_t id, QString name)
    : TChatChannelIdentifierBase(TChatChannelIdentifier::ChannelId)
    , m_id(id)
    , m_name(std::move(name))
{
}

TMessageToChatChannelIdentifier::TMessageToChatChannelIdentifier(QString partner)
    : TChatChannelIdentifierBase(TChatChannelIdentifier::MessageTo)
    , m_partner(std::move(partner))
{
}

TOwnPrivateChannelToOpenIdentifier::TOwnPrivateChannelToOpenIdentifier()
    : TChatChannelIdentifierBase(TChatChannelIdentifier::OwnPrivateChannelToOpen)
{
}

TLootChannelIdentifier::TLootChannelIdentifier()
    : TChatChannelIdentifierBase(TChatChannelIdentifier::Loot)
{
}

THelpChannelIdentifier::THelpChannelIdentifier()
    : TChannelIDChatChannelIdentifier(kChannelId)
{
}

namespace {

/// FUN_10099bc50: NPC keywords in braces become links in the keyword colour
/// (FUN_1009911a0); adjacent keywords merge first.
QString keywordLinks(QString text)
{
    static const QRegularExpression adjacent(QStringLiteral("\\}(\\s*)\\{"));
    static const QRegularExpression keyword(QStringLiteral("\\{([^\\}]*)\\}"));
    static const QString link = QStringLiteral("<a href=\"\\1\" style=\"color:%1; text-decoration:none\">\\1</a>")
                                    .arg(QColor(0x20, 0xa0, 0xff).name());
    text.replace(adjacent, QStringLiteral("\\1"));
    text.replace(keyword, link);
    return text;
}

}

QString formatLootRichText(QString text)
{
    static const QRegularExpression item(QStringLiteral("\\{([0-9]+)\\|([^\\}]*)\\}"));
    static const QRegularExpression colored(QStringLiteral("\\{(#[0-9a-fA-F]{6})\\|([^\\}]*)\\}"));
    cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
    if (prices && prices->colorizeLootStyle() != 0) {
        QString out;
        out.reserve(text.size() + 32);
        qsizetype pos = 0;
        QRegularExpressionMatchIterator it = item.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            out += text.mid(pos, match.capturedStart() - pos);
            const uint16_t typeId = static_cast<uint16_t>(match.captured(1).toUInt());
            out += prices->colouredItemName(typeId, match.captured(2));
            pos = match.capturedEnd();
        }
        out += text.mid(pos);
        text = std::move(out);
    } else {
        text.replace(item, QStringLiteral("\\2"));
    }
    text.replace(colored, QStringLiteral("<font color=\"\\1\">\\2</font>"));
    return text;
}

QString TChatChannelIdentifier::displayName() const
{
    switch (type) {
    case Local: return qtTrId("chat_channel_name_local_chat");
    case ServerLog: return qtTrId("chat_channel_name_server_log");
    case Npc: return qtTrId("chat_channel_name_npcs");
    case Loot: return qtTrId("chat_channel_name_loot");
    default: return name;
    }
}

bool TChatChannelIdentifier::sameChannel(const TChatChannelIdentifier& other) const
{
    if (type != other.type)
        return false;
    switch (type) {
    case ChannelId: return id == other.id;
    case MessageTo: return name.compare(other.name, Qt::CaseInsensitive) == 0;
    default: return true;
    }
}

TChatChannel::TChatChannel(const TChatChannelIdentifier& identifier, QObject* parent)
    : QObject(parent)
    , m_identifier(identifier)
{}

size_t TChatChannel::maxEntries()
{
    static const size_t value = static_cast<size_t>(chatSetting(QStringLiteral("maxChatBacklogLength"), 1000, 5));
    return value;
}

void TChatChannel::addEntry(TChatEntry entry)
{
    m_entries.push_back(std::move(entry));
    Q_EMIT entryAdded();
    if (m_entries.size() > maxEntries()) {
        m_entries.pop_front();
        Q_EMIT oldestEntryRemoved();
    }
}

void TChatChannel::clear()
{
    m_entries.clear();
    Q_EMIT cleared();
}

bool TChatChannel::canBeClosed() const
{
    return m_identifier.type != TChatChannelIdentifier::Local && m_identifier.type != TChatChannelIdentifier::ServerLog;
}

void TChatChannel::setSubscribedToServerLog(const bool subscribed)
{
    if (m_subscribedToServerLog == subscribed)
        return;
    m_subscribedToServerLog = subscribed;
    Q_EMIT propertiesChanged();
}

bool TChatChannel::writesToLocalChat() const
{
    switch (m_identifier.type) {
    case TChatChannelIdentifier::Npc:
    case TChatChannelIdentifier::ChannelId:
    case TChatChannelIdentifier::MessageTo:
        return false;
    default:
        return true;
    }
}

TChatChannelStorage::TChatChannelStorage(QObject* parent)
    : QObject(parent)
{
    reset();
}

TChatChannel* TChatChannelStorage::create(const TChatChannelIdentifier& identifier)
{
    switch (identifier.type) {
    case TChatChannelIdentifier::Local:
        (void)TLocalChatChannelIdentifier();
        break;
    case TChatChannelIdentifier::ServerLog:
        (void)TServerLogChannelIdentifier();
        break;
    case TChatChannelIdentifier::Npc:
        (void)TNpcChatChannelIdentifier();
        break;
    case TChatChannelIdentifier::ChannelId:
        if (identifier.id == THelpChannelIdentifier::kChannelId)
            (void)THelpChannelIdentifier();
        else
            (void)TChannelIDChatChannelIdentifier(identifier.id, identifier.name);
        break;
    case TChatChannelIdentifier::MessageTo:
        (void)TMessageToChatChannelIdentifier(identifier.name);
        break;
    case TChatChannelIdentifier::OwnPrivateChannelToOpen:
        (void)TOwnPrivateChannelToOpenIdentifier();
        break;
    case TChatChannelIdentifier::Loot:
        (void)TLootChannelIdentifier();
        break;
    }
    auto* channel = new TChatChannel(identifier, this);
    m_channels.push_back(channel);
    return channel;
}

void TChatChannelStorage::reset()
{
    for (TChatChannel* channel : m_channels)
        channel->deleteLater();
    m_channels.clear();
    create({ TChatChannelIdentifier::Local });
    create({ TChatChannelIdentifier::ServerLog });
    Q_EMIT channelsReset();
}

TChatChannel* TChatChannelStorage::find(const TChatChannelIdentifier& identifier) const
{
    for (TChatChannel* channel : m_channels) {
        if (channel->identifier().sameChannel(identifier))
            return channel;
    }
    return nullptr;
}

TChatChannel* TChatChannelStorage::ownChannel() const
{
    for (TChatChannel* channel : m_channels) {
        if (channel->isOwnChannel())
            return channel;
    }
    return nullptr;
}

TChatChannel* TChatChannelStorage::open(const TChatChannelIdentifier& identifier, const bool makeCurrent)
{
    if (TChatChannel* channel = find(identifier)) {
        if (channel->isClosedByServer()) {
            channel->setClosedByServer(false);
            Q_EMIT channel->propertiesChanged();
        }
        if (makeCurrent)
            Q_EMIT channelAdded(channel, true);
        return channel;
    }
    TChatChannel* channel = create(identifier);
    Q_EMIT channelAdded(channel, makeCurrent);
    return channel;
}

void TChatChannelStorage::close(TChatChannel* channel)
{
    if (!channel || !channel->canBeClosed())
        return;
    const auto it = std::find(m_channels.begin(), m_channels.end(), channel);
    if (it == m_channels.end())
        return;
    m_channels.erase(it);
    Q_EMIT channelRemoved(channel);
    channel->deleteLater();
}

void TChatChannelStorage::moveChannel(const int from, const int to)
{
    const int count = static_cast<int>(m_channels.size());
    if (from < 0 || to < 0 || from >= count || to >= count || from == to)
        return;
    TChatChannel* channel = m_channels[from];
    m_channels.erase(m_channels.begin() + from);
    m_channels.insert(m_channels.begin() + to, channel);
    Q_EMIT channelsMoved();
}

void TChatChannelStorage::add(TChatChannel* channel, TChatEntry entry)
{
    const bool fromOthers = entry.name.isEmpty() || entry.name.compare(m_ownName, Qt::CaseInsensitive) != 0;
    channel->addEntry(std::move(entry));
    Q_EMIT entryAddedToChannel(channel, fromOthers);
}

void TChatChannelStorage::addServerLogEntry(const QColor& color, const QString& text)
{
    const TChatEntry entry = TClientChatEntryFactory::systemEntry(text, color);
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            add(channel, entry);
    }
}

void TChatChannelStorage::addLocalEntry(const QColor& color, const QString& text, const QString& name, const int level, const uint32_t statementId)
{
    const TChatEntry::Kind kind = level == 0 ? TChatEntry::Speaker : TChatEntry::SpeakerWithLevel;
    add(localChat(), { kind, now(), text, name, level, false, color, statementId });
}

void TChatChannelStorage::addNpcEntry(const QColor& color, const QString& text, const QString& name)
{
    add(open({ TChatChannelIdentifier::Npc }, false), { TChatEntry::Speaker, now(), text, name, 0, false, color, 0 });
}

void TChatChannelStorage::addChannelEntry(const uint16_t channelId, const QColor& color, const QString& text, const QString& name,
                                          const int level, const uint32_t statementId)
{
    if (TChatChannel* channel = findChannelId(channelId))
        add(channel, { TChatEntry::SpeakerWithLevel, now(), text, name, level, false, color, statementId });
}

void TChatChannelStorage::addChannelSystemEntry(const uint16_t channelId, const QColor& color, const QString& text)
{
    if (TChatChannel* channel = findChannelId(channelId))
        add(channel, TClientChatEntryFactory::systemEntry(text, color));
}

void TChatChannelStorage::addPrivateEntry(const QString& partner, const QColor& color, const QString& text, const QString& name,
                                          const int level, const uint32_t statementId)
{
    TChatChannel* channel = find(TChatChannelIdentifier::messageTo(partner));
    if (!channel)
        channel = localChat();
    const TChatEntry::Kind kind = level == 0 ? TChatEntry::Speaker : TChatEntry::SpeakerWithLevel;
    add(channel, { kind, now(), text, name, level, false, color, statementId });
}

void TChatChannelStorage::addBroadcastEntry(const QColor& color, const QString& text, const QString& name, const int level,
                                            const uint32_t statementId)
{
    TChatEntry entry{ TChatEntry::SpeakerWithLevel, now(), text, name, level, false, color, statementId };
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            add(channel, entry);
    }
}

void TChatChannelStorage::addLootEntry(const QColor& color, const QString& text)
{
    if (TChatChannel* channel = find({ TChatChannelIdentifier::Loot }))
        add(channel, { TChatEntry::Loot, now(), text, {}, 0, false, color, 0 });
}

void TChatChannelStorage::addSystemEntry(TChatChannel* channel, const QColor& color, const QString& text)
{
    if (channel)
        add(channel, TClientChatEntryFactory::systemEntry(text, color));
}

QColor consoleColor(const int mode)
{
    switch (mode) {
    case 1: case 2: case 3: case 7: case 9:
        return QColor(0xf0, 0xf0, 0x00);
    case 4: case 10: case 11:
        return QColor(0x60, 0xf8, 0xf8);
    case 5: case 12: case 16:
        return QColor(0xa0, 0xa0, 0xff);
    case 6: case 22: case 31: case 32: case 34: case 39:
        return QColor(0x00, 0xf0, 0x00);
    case 8:
        return QColor(0xf0, 0xb4, 0x00);
    case 13: case 14: case 15: case 18: case 20:
        return QColor(0xf8, 0x60, 0x60);
    case 53:
        return QColor(0xff, 0x66, 0x00);
    default:
        return QColor(0xf0, 0xf0, 0xf0);
    }
}

QColor systemRed()
{
    return QColor(0xf8, 0x60, 0x60);
}

QColor systemWhite()
{
    return QColor(0xf0, 0xf0, 0xf0);
}

QString formatEntry(const TChatEntry& entry, const bool timestamps, const bool seconds, const bool levels)
{
    // FUN_100bd7a00: the prefix.
    QString prefix;
    if (timestamps)
        prefix = entry.time.toString(seconds ? QStringLiteral("hh:mm:ss") : QStringLiteral("hh:mm")) + QLatin1Char(' ');
    if (entry.kind == TChatEntry::Speaker || entry.kind == TChatEntry::SpeakerWithLevel) {
        prefix += entry.name;
        if (entry.traded && entry.kind == TChatEntry::SpeakerWithLevel)
            prefix += QStringLiteral(" (Traded)");
        if (entry.kind == TChatEntry::SpeakerWithLevel && levels)
            prefix += QStringLiteral(" [%1]").arg(entry.level);
        prefix += QStringLiteral(": ");
    }

    // FUN_100bd7ce0: the text.
    QString body = entry.text.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>\n"));
    if (entry.kind == TChatEntry::Speaker)
        body = keywordLinks(body);
    else if (entry.kind == TChatEntry::Loot)
        body = formatLootRichText(body);

    // FUN_10099b920, FUN_10099b870(10): CIP hanging indent + <font color>.
    return QStringLiteral("<p style=\"margin-left:10px; text-indent:-10px\"><font color=\"%1\">%2%3</font></p>")
        .arg(entry.color.name(), prefix.toHtmlEscaped(), body);
}

int chatSetting(const QString& key, const int defaultValue, const int minimum)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("CHAT"));
    const int value = settings.value(key, defaultValue).toInt();
    if (value >= minimum)
        return value;
    settings.setValue(key, minimum);
    return minimum;
}

}
