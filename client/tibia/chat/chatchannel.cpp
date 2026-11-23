#include "chatchannel.h"
#include "game/gamesessionbase.h"

#include "chat/chatgameactionhandler.h"
#include <stdexcept>
#include "input/gameactionhandlers.h"
#include "input/gameaction.h"
#include "creatures/creaturestorage.h"
#include "analysers/itempricestorage.h"
#include "config/clientoptions.h"

#include <tibia/game/tgamenotify.h>

#include <QTextDocumentFragment>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSettings>

#include <algorithm>

#include <stdexcept>

namespace tibia::chat {

TChatChannelIdentifier TChatChannelIdentifier::channel(const uint16_t id, const QString& name)
{
    return { ChannelId, id, name };
}

TChatChannelIdentifier TChatChannelIdentifier::messageTo(const QString& name)
{
    return { MessageTo, 0, name };
}

TChatChannelIdentifier TChatChannelIdentifier::help()
{
    return { ChannelId, 7, qtTrId("chat_channel_name_help") };
}

namespace {

/// : NPC keywords in braces become links in the keyword colour
/// ; adjacent keywords merge first.
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
    if (config::TClientOptions::instance()->colorizeLootStyle() != 0) {
        analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
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
    return replaceColorCodingsWithHtmlTags(text);
}

QString replaceColorCodingsWithHtmlTags(const QString& textWithColorCodings)
{
    static const QRegularExpression coding(QStringLiteral("\\{(#[0-9a-fA-F]{6})\\|([^\\}]*)\\}"));
    QString text = textWithColorCodings;
    qsizetype from = 0;
    for (QRegularExpressionMatch match = coding.match(text, from); match.hasMatch(); match = coding.match(text, from)) {
        const QString html = QStringLiteral("%1%2%3").arg(QStringLiteral("<font color=\"%1\">").arg(QColor::fromString(match.captured(1)).name())).arg(match.captured(2)).arg(QStringLiteral("</font>"));
        text.replace(match.capturedStart(), match.capturedLength(), html);
        from = match.capturedStart() + html.size();
    }
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
    case ChannelId: return id == other.id && displayName() == other.displayName();
    case MessageTo:
    case OwnPrivateChannelToOpen: return displayName() == other.displayName();
    default: return true;
    }
}

// ---------------------------------------------------------------------------
// TChatChannel

TChatChannel::TChatChannel(const uint32_t maxEntries, const TChatChannelIdentifier& identifier, QObject* parent)
    : cip::tibia::chat::TChatChannel(parent)
    , m_maxEntries(maxEntries)
    , m_identifier(identifier)
    , m_isSubscribedToServerLog(identifier.type == TChatChannelIdentifier::ServerLog)
{}

uint32_t TChatChannel::maxEntries()
{
    static const uint32_t value = static_cast<uint32_t>(chatSetting(QStringLiteral("maxChatBacklogLength"), 1000, 5));
    return value;
}

void TChatChannel::addEntry(const TChatEntry& entry)
{
    if (m_maxEntries <= entryCount() && !m_entries.empty()) {
        m_entries.pop_front();
        ++m_firstEntryId;
        dropStaleTutorialMarker();
    }
    m_entries.push_back(entry);
    Q_EMIT idRangeChanged(firstEntryId(), afterLastEntryId());
    Q_EMIT chatEntryAdded();
}

void TChatChannel::removeOldestEntry()
{
    if (entryCount() == 0)
        return;
    m_entries.pop_front();
    ++m_firstEntryId;
    Q_EMIT idRangeChanged(firstEntryId(), afterLastEntryId());
    dropStaleTutorialMarker();
}

void TChatChannel::clear()
{
    const uint32_t afterLast = afterLastEntryId();
    m_entries.clear();
    m_firstEntryId = afterLast;
    Q_EMIT idRangeChanged(firstEntryId(), afterLastEntryId());
    dropStaleTutorialMarker();
}

const TChatEntry& TChatChannel::entry(const uint32_t id) const
{
    return m_entries[id - m_firstEntryId];
}

void TChatChannel::setClosed(const bool closed)
{
    if (m_isClosed == closed)
        return;
    m_isClosed = closed;
    Q_EMIT isClosedChanged();
}

bool TChatChannel::isSubscribedToServerLog() const
{
    return m_isSubscribedToServerLog || m_identifier.type == TChatChannelIdentifier::ServerLog;
}

void TChatChannel::setSubscribedToServerLog(const bool subscribed)
{
    if (m_isSubscribedToServerLog == subscribed)
        return;
    m_isSubscribedToServerLog = subscribed;
    Q_EMIT serverLogSubscriptionChanged();
}

bool TChatChannel::addTutorialMarkerForKeyword(const int markerId, const QString& keyword)
{
    if (entryCount() == 0)
        return false;
    const QString needle = QStringLiteral("{%1}").arg(keyword);
    for (uint32_t id = firstEntryId(); id < afterLastEntryId(); ++id) {
        if (entry(id).text.indexOf(needle, 0, Qt::CaseInsensitive) != -1)
            m_tutorialMarker = std::make_shared<TKeywordGuiTutorialMarkerInfo>(TKeywordGuiTutorialMarkerInfo{ markerId, keyword, id });
    }
    const bool marked = m_tutorialMarker != nullptr;
    if (marked)
        Q_EMIT tutorialMarkerChanged();
    return marked;
}

void TChatChannel::removeTutorialMarker()
{
    m_tutorialMarker.reset();
    Q_EMIT tutorialMarkerChanged();
}

void TChatChannel::dropStaleTutorialMarker()
{
    if (m_tutorialMarker && m_tutorialMarker->entryId < firstEntryId())
        removeTutorialMarker();
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

// ---------------------------------------------------------------------------
// TChatChannelStorage

TChatChannelStorage::TChatChannelStorage(QObject* parent)
    : cip::tibia::chat::TChatChannelStorage(parent)
    , m_maxEntries(TChatChannel::maxEntries())
{
    openDefaultChannels();
}

void TChatChannelStorage::openDefaultChannels()
{
    m_localChat = open({ TChatChannelIdentifier::Local }, true, false);
    m_serverLog = open({ TChatChannelIdentifier::ServerLog }, true, false);
}

bool TChatChannelStorage::isOwnName(const QString& name) const
{
    return name.toLower() == m_ownName;
}

void TChatChannelStorage::onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<::tibia::creatures::TCreature> pPlayer)
{
    if (const auto player = pPlayer.lock())
        setOwnName(player->name());
}

void TChatChannelStorage::addServerLogEntry(const QColor& color, const QString& text)
{
    const TChatEntry entry = TClientChatEntryFactory::systemEntry(color, text);
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            channel->addEntry(entry);
    }
}

void TChatChannelStorage::addLootEntry(const QColor& color, const QString& text)
{
    const TChatEntry entry = TClientChatEntryFactory::lootEntry(color, text);
    if (m_lootChannel) {
        m_lootChannel->addEntry(entry);
        return;
    }
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            channel->addEntry(entry);
    }
}

void TChatChannelStorage::addLocalEntry(const QColor& color, const QString& name, const QString& text, const int level,
                                        const bool traded, const uint32_t statementId)
{
    if (level == 0)
        m_localChat->addEntry(TClientChatEntryFactory::speakerEntry(color, name, text));
    else
        m_localChat->addEntry(TClientChatEntryFactory::speakerWithLevelEntry(color, name, text, level, traded, statementId));
}

void TChatChannelStorage::addNpcChannelEntry(const QColor& color, const QString& name, const QString& text)
{
    open({ TChatChannelIdentifier::Npc }, false, false)->addEntry(TClientChatEntryFactory::speakerEntry(color, name, text));
    publishChatSound(cip::tibia::qmlcomponents::TSoundHelperService::CHAT_MESSAGE_ARRIVED, m_npcMessageSounds);
}

void TChatChannelStorage::addOwnNpcChannelEntry(const QColor& color, const QString& name, const QString& text, const int level)
{
    open({ TChatChannelIdentifier::Npc }, true, false)
        ->addEntry(TClientChatEntryFactory::speakerWithLevelEntry(color, name, text, level, false, 0));
}

void TChatChannelStorage::addNpcEntry(const QColor& color, const QString& text, const QString& name)
{
    open({ TChatChannelIdentifier::Npc }, true, false)->addEntry(TClientChatEntryFactory::speakerEntry(color, name, text));
}

void TChatChannelStorage::addChannelEntry(const QColor& color, const uint16_t channelId, const QString& name, const QString& text,
                                          const int level, const bool traded, const uint32_t statementId)
{
    TChatChannel* channel = findChannelId(channelId);
    if (!channel)
        return;
    channel->addEntry(TClientChatEntryFactory::speakerWithLevelEntry(color, name, text, level, traded, statementId));
    if (name.toLower() == m_ownName)
        return;
    const TChatChannelIdentifier& id = channel->identifier();
    // Team Finder is channel 5.
    const bool enabled = id.isPartyChannel() ? m_partyMessageSounds
                         : id.isGuildChannel() ? m_guildMessageSounds
                         : channelId == 5 ? m_teamFinderMessageSounds
                                          : true;
    publishChatSound(cip::tibia::qmlcomponents::TSoundHelperService::CHAT_MESSAGE_ARRIVED, enabled);
}

void TChatChannelStorage::addChannelSystemEntry(const QColor& color, const uint16_t channelId, const QString& text)
{
    const TChatEntry entry = TClientChatEntryFactory::systemEntry(color, text);
    if (TChatChannel* channel = findChannelId(channelId)) {
        channel->addEntry(entry);
        return;
    }
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            channel->addEntry(entry);
    }
}

void TChatChannelStorage::addChannelSystemEntryIfOpen(const QColor& color, const uint16_t channelId, const QString& text)
{
    if (TChatChannel* channel = findChannelId(channelId))
        channel->addEntry(TClientChatEntryFactory::systemEntry(color, text));
}

void TChatChannelStorage::addPrivateEntry(const QColor& color, const QString& partner, const QString& name, const QString& text,
                                          const int level, const bool traded, const uint32_t statementId)
{
    const TChatEntry entry = TClientChatEntryFactory::speakerWithLevelEntry(color, name, text, level, traded, statementId);
    TChatChannel* channel = find(TChatChannelIdentifier::messageTo(partner));
    int sound = cip::tibia::qmlcomponents::TSoundHelperService::CHAT_MESSAGE_ARRIVED;
    if (!channel) {
        channel = m_localChat;
        sound = cip::tibia::qmlcomponents::TSoundHelperService::PRIVATE_MESSAGE_IN_LOCAL_CHAT;
    }
    channel->addEntry(entry);
    if (name.toLower() != m_ownName)
        publishChatSound(sound, m_privateMessageSounds);
}

void TChatChannelStorage::setSoundOptions(const QJsonObject& options)
{
    const auto option = [&options](const char* key) { return options.value(QLatin1String(key)).toBool(true); };
    m_chatSoundsEnabled = option("ChatEnabled");
    m_partyMessageSounds = option("PartyMessagesEnabled");
    m_guildMessageSounds = option("GuildMessagesEnabled");
    m_privateMessageSounds = option("PrivateMessagesEnabled");
    m_npcMessageSounds = option("NpcMessageseEnabled");
    m_teamFinderMessageSounds = option("TeamFinderMessagesEnabled");
}

void TChatChannelStorage::publishChatSound(const int sound, const bool categoryEnabled)
{
    if (!m_chatSoundsEnabled || !categoryEnabled)
        return;
    Q_EMIT publishGameAction(
        std::make_shared<input::gameactions::TGameActionSoundTrigger>(input::gameactions::TGameActionSoundTrigger::TSoundEffect{ sound }));
}

void TChatChannelStorage::addBroadcastEntry(const QColor& color, const QString& name, const QString& text, const int level,
                                            const bool traded, const uint32_t statementId)
{
    const TChatEntry entry = TClientChatEntryFactory::speakerWithLevelEntry(color, name, text, level, traded, statementId);
    for (TChatChannel* channel : std::vector<TChatChannel*>(m_channels)) {
        if (channel->isSubscribedToServerLog())
            channel->addEntry(entry);
    }
}

TChatChannel* TChatChannelStorage::open(const TChatChannelIdentifier& identifier, const bool makeCurrentOnOpen,
                                        const bool forceMakeCurrent)
{
    if (TChatChannel* channel = find(identifier)) {
        if (channel->isClosed())
            channel->setClosed(false);
        Q_EMIT chatChannelReopend(identifier, forceMakeCurrent);
        if (forceMakeCurrent || channel->makeCurrentOnOpen())
            Q_EMIT channelAdded(channel, true);
        if (identifier.type == TChatChannelIdentifier::MessageTo)
            channel->setMakeCurrentOnOpen(true);
        return channel;
    }
    auto* channel = new TChatChannel(m_maxEntries, identifier, this);
    channel->setMakeCurrentOnOpen(makeCurrentOnOpen);
    if (identifier.type == TChatChannelIdentifier::ChannelId) {
        if (identifier.isHelpChannel())
            channel->addEntry(TClientChatEntryFactory::helpChannelIntroductionEntry());
        else if (identifier.isAdvertisingChannel())
            channel->addEntry(TClientChatEntryFactory::advertisingChannelIntroductionEntry());
    } else if (identifier.type == TChatChannelIdentifier::Loot) {
        m_lootChannel = channel;
    }
    m_channels.push_back(channel);
    connect(channel, &TChatChannel::chatEntryAdded, this, [this, channel] {
        Q_EMIT entryAddedToChatChannel(channel->identifier());
        const QString& name = channel->entries().back().name;
        Q_EMIT entryAddedToChannel(channel, name.isEmpty() || !isOwnName(name));
    });
    Q_EMIT newChatChannelOpened(identifier, forceMakeCurrent);
    Q_EMIT channelAdded(channel, forceMakeCurrent || channel->makeCurrentOnOpen());
    return channel;
}

void TChatChannelStorage::openServerChannel(const uint16_t channelId, const QString& name, const bool ownPrivateChannel,
                                            const QStringList& participants)
{
    TChatChannel* channel = open(TChatChannelIdentifier::channel(channelId, name), true, false);
    channel->setOwnPrivateChannel(ownPrivateChannel);
    if (!participants.isEmpty())
        channel->addEntry(TClientChatEntryFactory::systemEntry(
            systemWhite(), qtTrId("chat_channel_participants").arg(participants.join(QStringLiteral(", ")))));
}

void TChatChannelStorage::openPrivateChannel(const QString& partner, const bool makeCurrentOnOpen)
{
    open(TChatChannelIdentifier::messageTo(partner), makeCurrentOnOpen, false);
}

void TChatChannelStorage::closeChannelByServer(const uint16_t channelId)
{
    TChatChannel* channel = findChannelId(channelId);
    if (!channel)
        return;
    const TChatChannelIdentifier& identifier = channel->identifier();
    const bool teamFinder = identifier.type == TChatChannelIdentifier::ChannelId && identifier.isTeamFinderChannel();
    channel->addEntry(teamFinder ? TClientChatEntryFactory::teamFinderChannelClosedEntry()
                                 : TClientChatEntryFactory::channelClosedEntry());
    channel->setClosed(true);
}

void TChatChannelStorage::clearAllChannels()
{
    for (TChatChannel* channel : m_channels)
        channel->clear();
}

void TChatChannelStorage::removeChannel(const TChatChannelIdentifier& identifier)
{
    if (!identifier.canBeClosed())
        return;
    const auto it = std::find_if(m_channels.begin(), m_channels.end(),
                                 [&](const TChatChannel* channel) { return channel->identifier().sameChannel(identifier); });
    if (it == m_channels.end())
        return;
    TChatChannel* channel = *it;
    Q_EMIT chatChannelRemoved(channel->identifier());
    if (identifier.type == TChatChannelIdentifier::Loot)
        m_lootChannel = nullptr;
    m_channels.erase(it);
    Q_EMIT channelRemoved(channel);
    channel->deleteLater();
}

TChatChannel* TChatChannelStorage::find(const TChatChannelIdentifier& identifier) const
{
    for (TChatChannel* channel : m_channels) {
        if (channel->identifier().sameChannel(identifier))
            return channel;
    }
    return nullptr;
}

TChatChannel* TChatChannelStorage::findChannelId(const uint16_t channelId) const
{
    for (TChatChannel* channel : m_channels) {
        const TChatChannelIdentifier& identifier = channel->identifier();
        if (identifier.type == TChatChannelIdentifier::ChannelId && !channel->isClosed() && identifier.id == channelId)
            return channel;
    }
    return nullptr;
}

TChatChannel* TChatChannelStorage::ownChannel() const
{
    for (TChatChannel* channel : m_channels) {
        const TChatChannelIdentifier& identifier = channel->identifier();
        if (identifier.type == TChatChannelIdentifier::ChannelId && channel->isOwnPrivateChannel()
            && identifier.isPrivateChannelId())
            return channel;
    }
    return nullptr;
}

std::vector<TChatChannelIdentifier> TChatChannelStorage::identifiers() const
{
    std::vector<TChatChannelIdentifier> result;
    result.reserve(channelCount());
    for (const TChatChannel* channel : m_channels)
        result.push_back(channel->identifier());
    return result;
}

void TChatChannelStorage::addTutorialMarkerToNpcChannel(const int markerId, const QString& keyword)
{
    if (TChatChannel* channel = find({ TChatChannelIdentifier::Npc }))
        channel->addTutorialMarkerForKeyword(markerId, keyword);
}

void TChatChannelStorage::removeAllTutorialMarkers()
{
    for (TChatChannel* channel : m_channels)
        channel->removeTutorialMarker();
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

void TChatChannelStorage::addSystemEntry(TChatChannel* channel, const QColor& color, const QString& text)
{
    if (channel)
        channel->addEntry(TClientChatEntryFactory::systemEntry(color, text));
}

int chatMessageMode(const uint32_t protocolMode)
{
    if (protocolMode < 0x37 && ((0x7f1dffffffffffULL >> protocolMode) & 1) != 0)
        return static_cast<int>(protocolMode);
    throw std::runtime_error(QStringLiteral("EChatmessageMode %1 couldn't be converted").arg(protocolMode).toStdString());
}

uint32_t protocolMessageMode(const int chatMode)
{
    const auto mode = static_cast<uint8_t>(chatMode);
    if (mode < 0x37 && ((0x7f1dffffffffffULL >> mode) & 1) != 0)
        return mode;
    throw std::runtime_error(QStringLiteral("EChatmessageMode %1 couldn't be converted").arg(mode).toStdString());
}

QColor consoleColor(const int mode)
{
    const unsigned index = static_cast<unsigned>(mode - 1);
    if (index >= 0x35 || ((0x178c67ffefffffULL >> index) & 1) == 0)
        throw std::runtime_error(QStringLiteral("No chat channel color selected for EChatmessageMode %1!").arg(mode).toStdString());
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
    case 17: case 19: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
    case 33: case 35: case 38: case 43: case 44: case 48: case 49: case 50: case 51:
        return QColor(0xf0, 0xf0, 0xf0);
    default:
        throw std::runtime_error(
            QStringLiteral("No chat channel color selected for EChatmessageMode %1!").arg(mode).toStdString());
    }
}

QColor overlayColor(const int mode)
{
    const unsigned index = static_cast<unsigned>(mode - 1);
    if (index >= 0x36 || ((0x3f087ae03f570fULL >> index) & 1) == 0)
        throw std::runtime_error(QStringLiteral("No text overlay color selected for EChatmessageMode %1!").arg(mode).toStdString());
    switch (mode) {
    case 1: case 2: case 3: case 9:
        return QColor(0xf0, 0xf0, 0x00);
    case 4: case 10: case 11:
        return QColor(0x60, 0xf8, 0xf8);
    case 13: case 15: case 18: case 20:
        return QColor(0xf8, 0x60, 0x60);
    case 22: case 31: case 32: case 34: case 39:
        return QColor(0x00, 0xf0, 0x00);
    case 36: case 37: case 52: case 53:
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

QString formatEntry(const TChatEntry& entry, const bool timestamps, const bool seconds, const bool levels, int* plainLength)
{
    // The prefix: time, speaker, traded mark and level.
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

    // The text: escaped, keyword links for speakers, item colours for loot.
    QString body = entry.text.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>\n"));
    if (entry.kind == TChatEntry::Speaker)
        body = keywordLinks(body);
    else if (entry.kind == TChatEntry::Loot)
        body = formatLootRichText(body);
    if (plainLength)
        *plainLength = static_cast<int>(prefix.size() + QTextDocumentFragment::fromHtml(body).toPlainText().size());

    // The entry's colour and a hanging indent of 10.
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
