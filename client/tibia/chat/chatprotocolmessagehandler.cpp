#include "chatprotocolmessagehandler.h"
#include "game/gamesessionbase.h"
#include <client/staticdata.h>

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "client/animatedtext.h"
#include "protocol/protocolciptypes.h"
#include "config/clientoptiondefaults.h"
#include "creatures/creaturestorage.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "shared/tibiaurltools.h"
#include "chat/textstorage.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"

#include <tibia/game/gamesession.h>
#include "protocol/protocolcodes.h"

#include <memory>
#include <stdexcept>

#include <QJsonArray>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <protocol.pb.h>

namespace tibia::chat {

namespace {

using client::gamecore::argInt;
using client::gamecore::argString;
using Args = std::vector<std::any>;

/// CIP's message modes: the protocol's (identity table).
enum Mode : int
{
    Say = 1,
    Whisper = 2,
    Yell = 3,
    PrivateFrom = 4,
    ChannelManagement = 6,
    Channel = 7,
    ChannelHighlight = 8,
    Spell = 9,
    NpcFromStartBlock = 10,
    NpcFrom = 11,
    NpcTo = 12,
    GamemasterBroadcast = 13,
    GamemasterChannel = 14,
    GamemasterPrivateFrom = 15,
    Login = 17,
    Admin = 18,
    Game = 19,
    GameHighlight = 20,
    Failure = 21,
    Look = 22,
    DamageDealt = 23,
    DamageReceived = 24,
    Heal = 25,
    Experience = 26,
    DamageOthers = 27,
    HealOthers = 28,
    ExperienceOthers = 29,
    Status = 30,
    Loot = 31,
    TradeNpc = 32,
    Guild = 33,
    PartyManagement = 34,
    Party = 35,
    MonsterSay = 36,
    MonsterYell = 37,
    Report = 38,
    HotkeyUse = 39,
    Mana = 43,
    BeyondLast = 44,
    Attention = 48,
    BoostedCreature = 49,
    OfflineTraining = 50,
    Transaction = 51,
    Potion = 52,
    Mode53 = 53,
    Mode54 = 54,
};

int protocolMode(const Args& args, const size_t index)
{
    if (index < args.size())
        if (const auto* mode = std::any_cast<tibia::client::MessageMode>(&args[index]))
            return Proto::translateMessageModeToServer(*mode);
    return -1;
}

/// Missing key → CIP default from (option id table; 0x1a
/// openNewPrivateMessagesInNewTab = 0, 0x1b..0x20 / 0x45..0x46 console and
/// spell filters = 1).
bool option(const QJsonObject& options, const char* name)
{
    const QLatin1String key(name);
    return options.value(key).toBool(config::defaultClientOptions().value(key).toBool());
}

QJsonObject whiteBlacklistOptions()
{
    return app::ClientOptions::load().root().value(QLatin1String("whiteBlacklistOptions")).toObject();
}

}

TWhiteBlacklist::TWhiteBlacklist(QObject* parent)
    : cip::tibia::chat::TWhiteBlacklist(parent)
{
    loadFromOptions();
}

TWhiteBlacklist::Entries::iterator TWhiteBlacklist::findEntry(Entries& entries, const QString& name)
{
    const QString lower = name.toLower();
    return std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.name.toLower() == lower; });
}

TWhiteBlacklist::Entries::const_iterator TWhiteBlacklist::findEntry(const Entries& entries, const QString& name)
{
    const QString lower = name.toLower();
    return std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.name.toLower() == lower; });
}

void TWhiteBlacklist::insertEntry(Entries& entries, const Entry& entry)
{
    if (const auto it = findEntry(entries, entry.name); it != entries.end())
        entries.erase(it);
    entries.push_back(entry);
}

void TWhiteBlacklist::sortEntries(Entries& entries)
{
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.name.toLower().compare(b.name.toLower()) < 0; });
}

bool TWhiteBlacklist::isVip(const QString& name) const
{
    return m_isVip && m_isVip(name);
}

void TWhiteBlacklist::addToBlacklist(const QString& name, const bool permanently)
{
    insertEntry(m_blacklist, { name, permanently });
    sortEntries(m_blacklist);
    saveToOptions();
}

void TWhiteBlacklist::removeFromBlacklist(const QString& name)
{
    const auto it = findEntry(m_blacklist, name);
    if (it == m_blacklist.end())
        return;
    m_blacklist.erase(it);
    saveToOptions();
}

void TWhiteBlacklist::addToWhitelist(const QString& name, const bool permanently)
{
    insertEntry(m_whitelist, { name, permanently });
    sortEntries(m_whitelist);
    saveToOptions();
}

void TWhiteBlacklist::setLists(const Entries& whitelist, const Entries& blacklist)
{
    m_whitelist.clear();
    for (const Entry& entry : whitelist)
        insertEntry(m_whitelist, entry);
    sortEntries(m_whitelist);
    m_blacklist.clear();
    for (const Entry& entry : blacklist)
        insertEntry(m_blacklist, entry);
    sortEntries(m_blacklist);
    saveToOptions();
}

void TWhiteBlacklist::loadFromOptions()
{
    const QJsonObject options = whiteBlacklistOptions();
    const auto load = [](const QJsonArray& names, Entries& entries) {
        entries.clear();
        for (const QJsonValue& value : names) {
            const QString name = value.toString();
            if (!name.isEmpty())
                insertEntry(entries, { name, true });
        }
        sortEntries(entries);
    };
    load(options.value(QLatin1String("blacklistedNames")).toArray(), m_blacklist);
    load(options.value(QLatin1String("whitelistedNames")).toArray(), m_whitelist);
}

void TWhiteBlacklist::saveToOptions() const
{
    const auto permanentNames = [](const Entries& entries) {
        QJsonArray names;
        for (const Entry& entry : entries)
            if (entry.permanently)
                names.append(entry.name);
        return names;
    };
    QJsonObject options = whiteBlacklistOptions();
    options.insert(QLatin1String("blacklistedNames"), permanentNames(m_blacklist));
    options.insert(QLatin1String("whitelistedNames"), permanentNames(m_whitelist));
    app::ClientOptions::storeAll({ { QStringLiteral("whiteBlacklistOptions"), options } });
}

bool TWhiteBlacklist::blocksName(const QString& name) const
{
    return m_blacklistActive && !isWhitelisted(name) && isOnBlacklist(name);
}

bool TWhiteBlacklist::blocksYell(const QString& name) const
{
    return m_blacklistActive && m_ignoreYelling && !isWhitelisted(name);
}

bool TWhiteBlacklist::blocksPrivateMessage(const QString& name) const
{
    return m_blacklistActive && m_ignorePrivateMessages && !isWhitelisted(name);
}

bool TWhiteBlacklist::isBlacklisted(const QString& name)
{
    if (!blocksName(name))
        return false;
    Q_EMIT messageFiltered();
    return true;
}

bool TWhiteBlacklist::filtersYell(const QString& name)
{
    if (!blocksYell(name))
        return false;
    Q_EMIT messageFiltered();
    return true;
}

bool TWhiteBlacklist::filtersPrivateMessage(const QString& name)
{
    if (!blocksPrivateMessage(name))
        return false;
    Q_EMIT messageFiltered();
    return true;
}

bool TWhiteBlacklist::isOnBlacklist(const QString& name) const
{
    return findEntry(m_blacklist, name) != m_blacklist.end();
}

bool TWhiteBlacklist::isWhitelisted(const QString& name) const
{
    if (!m_whitelistActive)
        return false;
    return findEntry(m_whitelist, name) != m_whitelist.end() || (m_allowVips && isVip(name));
}

bool TWhiteBlacklist::isExemptMode(const int mode)
{
    return ((0x2000300000ec00ULL >> (static_cast<uint8_t>(mode) & 0x3f)) & 1) != 0 && static_cast<uint8_t>(mode) < 0x36;
}

void TWhiteBlacklist::setFlags(const bool blacklistActive, const bool whitelistActive, const bool ignoreYelling,
                               const bool ignorePrivateMessages, const bool allowVips)
{
    setBlacklistActive(blacklistActive);
    setWhitelistActive(whitelistActive);
    setIgnoreYelling(ignoreYelling);
    setIgnorePrivateMessages(ignorePrivateMessages);
    setAllowVips(allowVips);
}

TChatProtocolMessageHandler::TChatProtocolMessageHandler(TChatChannelStorage& storage, TChatStorage& chatStorage,
                                                         TWhiteBlacklist& blacklist, QObject* parent)
    : cip::tibia::chat::TChatProtocolMessageHandler(parent)
    , m_storage(storage)
    , m_chatStorage(chatStorage)
    , m_blacklist(blacklist)
{
    s_console = this;
    // CIP onChatProtocolMessageHandlerOptionsChanged .
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            [this](const QString& name) {
                if (name.startsWith(QLatin1String("console")) || name.startsWith(QLatin1String("gameWindowShow"))
                    || name == QLatin1String("openNewPrivateMessagesInNewTab") || name.endsWith(QLatin1String("MessagesEnabled"))
                    || name == QLatin1String("ChatEnabled") || name == QLatin1String("colorizeLootStyle")) {
                    onChatProtocolMessageHandlerOptionsChanged();
                }
            });
    // CIP onPlayerCreatureAddedToCreatureStorage .
    tibia::game::onEachGameSession(this, [this] {
        if (creatures::TCreatureStorage* creatures = creatures::TCreatureStorage::instance()) {
            connect(creatures, &creatures::TCreatureStorage::playerAdded, this,
                    &TChatProtocolMessageHandler::onPlayerCreatureAddedToCreatureStorage, Qt::UniqueConnection);
        }
    });
    onChatProtocolMessageHandlerOptionsChanged();
}

TChatProtocolMessageHandler* TChatProtocolMessageHandler::s_console = nullptr;

TChatProtocolMessageHandler::~TChatProtocolMessageHandler()
{
    if (s_console == this)
        s_console = nullptr;
}

void TChatProtocolMessageHandler::setOptions(const QJsonObject& options)
{
    m_showEventMessages = option(options, "consoleShowEventMessages");
    m_showInfoMessages = option(options, "consoleShowInfoMessages");
    m_showStatusMessagesOwn = option(options, "consoleShowStatusMessagesOwn");
    m_showStatusMessagesOfOthers = option(options, "consoleShowStatusMessagesOfOthers");
    m_showJoinLeaveMessages = option(options, "consoleShowJoinLeaveMessages");
    m_openPrivateMessagesInNewTab = option(options, "openNewPrivateMessagesInNewTab");
    m_showOwnSpells = option(options, "gameWindowShowOwnSpells");
    m_showOthersSpells = option(options, "gameWindowShowOthersSpells");
    m_showTextualEffects = option(options, "gameWindowShowTextualEffects");
    m_colorizeLootStyle = options.value(QLatin1String("colorizeLootStyle"))
                              .toInt(config::defaultClientOptions().value(QLatin1String("colorizeLootStyle")).toInt());
    m_showMessages = option(options, "gameWindowShowMessages");
    m_showPrivateMessages = option(options, "gameWindowShowPrivateMessages");
    m_showBoostedCreatureMessages = option(options, "gameWindowShowBoostedCreatureMessages");
    m_showOfflineTrainingMessages = option(options, "gameWindowShowOfflineTrainingMessages");
    m_showStoreMessages = option(options, "gameWindowShowStoreMessages");
    m_showPotionMessages = option(options, "gameWindowShowPotionMessages");
    m_showHotkeyUsageMessages = option(options, "gameWindowShowHotkeyUsageMessages");
    m_showLootMessages = option(options, "gameWindowShowLootMessages");
    m_storage.setSoundOptions(options);
}

void TChatProtocolMessageHandler::onChatProtocolMessageHandlerOptionsChanged()
{
    // — options object is the CIP-named console / UI-Sounds section.
    setOptions(app::ClientOptions::load().options());
}

void TChatProtocolMessageHandler::onPlayerCreatureAddedToCreatureStorage(
    std::weak_ptr<::tibia::creatures::TCreature> pPlayer)
{
    // : store lower-cased own name for filter / arrived-sound checks.
    if (const auto player = pPlayer.lock())
        m_storage.setOwnName(player->name());
}

namespace {

/// CIP's speech volume of a chat message mode.
int speechVolume(const int mode)
{
    switch (mode) {
        case Say: return TCreatureSpeechBubble::Say;
        case Whisper: return TCreatureSpeechBubble::Whisper;
        case Yell: return TCreatureSpeechBubble::Yell;
        case Spell: return TCreatureSpeechBubble::Cast;
        case MonsterSay:
        case MonsterYell:
        case Potion:
        case Mode53: return TCreatureSpeechBubble::Monster;
        default:
            throw std::runtime_error(QStringLiteral("EChatmessageMode %1 has no speech volume!").arg(mode).toStdString());
    }
}

Position speechPosition(const ::tibia::protobuf::protocol::GameserverMessageTalk& message)
{
    if (!message.has_position())
        return {};
    const auto& c = message.position();
    return Position(static_cast<uint16_t>(c.x()), static_cast<uint16_t>(c.y()), static_cast<uint8_t>(c.z()));
}

}

void TChatProtocolMessageHandler::handleTalkMessage(const ::tibia::protobuf::protocol::GameserverMessageTalk& TalkMessage)
{
    int mode = 0;
    try {
        mode = chatMessageMode(TalkMessage.mode());
    } catch (const std::exception&) {
        throw std::runtime_error(QStringLiteral("Received invalid chat message mode (%1) while handling talk network message.")
                                     .arg(TalkMessage.mode())
                                     .toStdString());
    }
    const QString name = QString::fromStdString(TalkMessage.speaker());
    const QString text = shared::TTibiaUrlTools::process(QString::fromStdString(TalkMessage.text()));
    const int level = static_cast<uint16_t>(TalkMessage.speaker_level());
    const uint32_t statementId = TalkMessage.statement_id();
    const bool traded = TalkMessage.traded();
    if (!TWhiteBlacklist::isExemptMode(mode) && m_blacklist.isBlacklisted(name))
        return;
    switch (mode) {
        case Yell:
            if (m_blacklist.filtersYell(name))
                return;
            [[fallthrough]];
        case Say:
        case Whisper:
            addLocalChatEntry(name, level, traded, statementId, speechVolume(mode), consoleColor(mode), text);
            showCreatureSpeech(name, mode, text, speechPosition(TalkMessage));
            break;
        case PrivateFrom:
            if (m_blacklist.filtersPrivateMessage(name))
                return;
            addPrivateChatEntry(name, level, traded, statementId, text, consoleColor(mode));
            if (m_showMessages && m_showPrivateMessages && m_textStorage)
                m_textStorage->addPrivateMessage(name, text, overlayColor(mode));
            break;
        case Channel:
        case ChannelHighlight:
        case GamemasterChannel:
            m_storage.addChannelEntry(consoleColor(mode), static_cast<uint16_t>(TalkMessage.channel_id()), name, text, level, traded,
                                      statementId);
            break;
        case Spell: {
            addLocalChatEntry(name, level, traded, statementId, speechVolume(mode), consoleColor(mode), text);
            const bool own = name.toLower() == m_storage.ownName();
            if (own ? m_showOwnSpells : m_showOthersSpells)
                showCreatureSpeech(name, mode, text, speechPosition(TalkMessage));
            break;
        }
        case NpcFromStartBlock:
            if (m_showMessages && m_textStorage) {
                m_textStorage->setNpcSpeechHeader(name, overlayColor(mode), speechPosition(TalkMessage));
                m_textStorage->addNpcSpeech(name, text);
            }
            m_storage.addNpcChannelEntry(consoleColor(mode), name, text);
            break;
        case NpcFrom:
            if (m_showMessages && m_textStorage)
                m_textStorage->addNpcSpeech(name, text);
            m_storage.addNpcChannelEntry(consoleColor(mode), name, text);
            break;
        case GamemasterBroadcast:
        case GamemasterPrivateFrom:
            m_storage.addBroadcastEntry(consoleColor(mode), name, text, level, traded, statementId);
            if (m_showMessages && m_textStorage)
                m_textStorage->addGamemasterMessage(name, text, overlayColor(mode));
            break;
        case Game:
            if (m_showMessages && m_textStorage)
                m_textStorage->addCenterMessage(text, overlayColor(mode));
            m_storage.addServerLogEntry(consoleColor(mode), text);
            break;
        case MonsterSay:
        case MonsterYell:
            showCreatureSpeech(name, mode, text, speechPosition(TalkMessage));
            break;
        case Potion:
            if (m_showPotionMessages)
                showCreatureSpeech(name, mode, text, speechPosition(TalkMessage));
            break;
        case Mode53:
            showCreatureSpeech(name, mode, text, speechPosition(TalkMessage));
            if (TChatChannel* local = m_storage.localChat())
                local->addEntry(TClientChatEntryFactory::systemEntry(consoleColor(mode), text));
            break;
        case NpcTo:
            // Host (IdleOT): quest expeditions send the player's lines to an
            // NPC; they belong on the NPCs tab with the NPC's replies.
            m_storage.addNpcChannelEntry(consoleColor(mode), name, text);
            break;
        default:
            break;
    }
}

void TChatProtocolMessageHandler::addLocalChatEntry(const QString& name, const int level, const bool traded, const uint32_t statementId,
                                                    const int volume, const QColor& color, const QString& text)
{
    m_storage.addLocalEntry(color, name, volume == TCreatureSpeechBubble::Yell ? text.toUpper() : text, level, traded, statementId);
}

void TChatProtocolMessageHandler::addPrivateChatEntry(const QString& name, const int level, const bool traded,
                                                      const uint32_t statementId, const QString& text, const QColor& color)
{
    if (m_openPrivateMessagesInNewTab && !m_storage.find(TChatChannelIdentifier::messageTo(name)) && !isOwnName(name)) {
        m_storage.openPrivateChannel(name, false);
        Q_EMIT privateChannelOpened(name);
    }
    m_storage.addPrivateEntry(color, name, name, text, level, traded, statementId);
}

void TChatProtocolMessageHandler::showCreatureSpeech(const QString& name, const int mode, const QString& text, const Position& position)
{
    const int volume = speechVolume(mode);
    if (m_showMessages && m_textStorage)
        m_textStorage->addCreatureSpeech(name, volume, text, overlayColor(mode), position);
}

namespace {

/// "<name> invites you to …" → name.
QString invitingName(const QString& text)
{
    const qsizetype at = text.indexOf(QLatin1String(" invites you to "));
    return at == -1 ? QString() : text.mid(0, at);
}

/// "You have been excluded from the channel <name>'s Channel." → name.
QString excludingName(const QString& text)
{
    static const QLatin1String prefix("You have been excluded from the channel ");
    const qsizetype start = text.indexOf(prefix);
    if (start == -1)
        return {};
    const qsizetype end = text.indexOf(QLatin1String("'s Channel."));
    if (end == -1)
        return {};
    const qsizetype from = start + prefix.size();
    return text.mid(from, end - from);
}

}

void TChatProtocolMessageHandler::handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& MessageMessage)
{
    int mode = 0;
    try {
        mode = chatMessageMode(MessageMessage.mode());
    } catch (const std::exception&) {
        throw std::runtime_error(QStringLiteral("Received invalid chat message mode (%1) while handling message network message.")
                                     .arg(MessageMessage.mode())
                                     .toStdString());
    }
    const QString text = shared::TTibiaUrlTools::process(QString::fromStdString(MessageMessage.text()));
    const auto statusMessage = [&](const QString& line, const QColor& color) {
        if (m_textStorage && m_showMessages)
            m_textStorage->setStatusMessage(line, color, false);
    };
    const auto centerMessage = [&] {
        if (m_textStorage && m_showMessages)
            m_textStorage->addCenterMessage(text, overlayColor(mode));
    };
    const auto lookMessage = [&] {
        if (m_textStorage && m_showMessages)
            m_textStorage->setLookMessage(text, overlayColor(mode), false);
    };
    const auto numericalEffect = [&](const int64_t value, const uint32_t color) {
        if (m_effects && m_showTextualEffects && value != 0 && MessageMessage.has_position())
            m_effects->addNumericalEffect(std::make_shared<effects::TNumericalEffect>(
                Position(static_cast<uint16_t>(MessageMessage.position().x()), static_cast<uint16_t>(MessageMessage.position().y()),
                         static_cast<uint8_t>(MessageMessage.position().z())),
                value, effects::numericalEffectColorFrom8bit(static_cast<int>(color)), TTextStorage::now()));
    };
    bool log = false;
    switch (mode) {
    case ChannelManagement: {
        QString name = invitingName(text);
        if (name.isEmpty())
            name = excludingName(text);
        if (TWhiteBlacklist::isExemptMode(mode) || !m_blacklist.isBlacklisted(name))
            m_storage.addChannelSystemEntry(consoleColor(mode), static_cast<uint16_t>(MessageMessage.channel_id()), text);
        break;
    }
    case Login:
    case Status:
        statusMessage(text, overlayColor(mode));
        log = true;
        break;
    case Failure:
    case Mode54:
        statusMessage(text, overlayColor(mode));
        break;
    case Admin:
    case Game:
    case GameHighlight:
    case Report:
        centerMessage();
        log = true;
        break;
    case Look:
    case TradeNpc:
    case BeyondLast:
        lookMessage();
        log = true;
        break;
    case DamageDealt:
    case DamageReceived:
    case DamageOthers:
        numericalEffect(MessageMessage.value(), MessageMessage.color());
        numericalEffect(MessageMessage.second_value(), MessageMessage.second_color());
        log = true;
        break;
    case Heal:
    case Experience:
    case HealOthers:
    case ExperienceOthers:
    case Mana:
        numericalEffect(MessageMessage.value(), MessageMessage.color());
        log = true;
        break;
    case Loot:
        handleLootMessageForChannelAndStatusMessage(text);
        break;
    case Guild:
    case Party:
        m_storage.addChannelSystemEntry(consoleColor(mode), static_cast<uint16_t>(MessageMessage.channel_id()), text);
        break;
    case PartyManagement:
        m_storage.addChannelSystemEntry(consoleColor(mode), static_cast<uint16_t>(MessageMessage.channel_id()), text);
        centerMessage();
        break;
    case HotkeyUse:
        if (m_showHotkeyUsageMessages)
            lookMessage();
        log = true;
        break;
    case Attention:
        log = true;
        break;
    case BoostedCreature:
        if (m_showBoostedCreatureMessages)
            centerMessage();
        log = true;
        break;
    case OfflineTraining:
        if (m_showOfflineTrainingMessages)
            centerMessage();
        log = true;
        break;
    case Transaction:
        if (m_showStoreMessages)
            centerMessage();
        log = true;
        break;
    default:
        break;
    }
    if (log && !consoleFilters(mode))
        m_storage.addServerLogEntry(consoleColor(mode), text);
    // IdleOT host extension: packs observe the server's text messages.
    tibia::game::notify("onServerTextMessage", Proto::translateMessageModeFromServer(static_cast<uint8_t>(MessageMessage.mode())),
                        MessageMessage.text());
}

void TChatProtocolMessageHandler::handleLootMessageForChannelAndStatusMessage(const QString& text)
{
    const QColor color = m_colorizeLootStyle != 0 ? QColor(Qt::white) : overlayColor(Loot);
    if (m_textStorage && m_showLootMessages && m_showMessages)
        m_textStorage->setLookMessage(formatLootRichText(text), color, true);
    if (m_storage.lootChannel() || m_showInfoMessages)
        m_storage.addLootEntry(color, text);
}

bool TChatProtocolMessageHandler::consoleFilters(const int mode) const
{
    switch (mode) {
    case Login:
    case Game:
        return !m_showEventMessages;
    case Look:
    case Loot:
    case TradeNpc:
    case HotkeyUse:
        return !m_showInfoMessages;
    case DamageDealt:
    case DamageReceived:
    case Heal:
    case Experience:
    case Status:
    case Mana:
        return !m_showStatusMessagesOwn;
    case DamageOthers:
    case HealOthers:
    case ExperienceOthers:
        return !m_showStatusMessagesOfOthers;
    default:
        return false;
    }
}

void TChatProtocolMessageHandler::handleOpenChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenChannel& OpenChannelMessage)
{
    QStringList participants;
    for (const auto& name : OpenChannelMessage.joined_players())
        participants.append(QString::fromStdString(name));
    m_storage.openServerChannel(static_cast<uint16_t>(OpenChannelMessage.channel_id()),
                                QString::fromStdString(OpenChannelMessage.name()), false, participants);
}

void TChatProtocolMessageHandler::handleOpenOwnChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenOwnChannel& OpenChannelMessage)
{
    QStringList participants;
    for (const auto& name : OpenChannelMessage.joined_players())
        participants.append(QString::fromStdString(name));
    m_storage.openServerChannel(static_cast<uint16_t>(OpenChannelMessage.channel_id()),
                                QString::fromStdString(OpenChannelMessage.name()), true, participants);
}

void TChatProtocolMessageHandler::handleCloseChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseChannel& CloseChannelMessage)
{
    m_storage.closeChannelByServer(static_cast<uint16_t>(CloseChannelMessage.channel_id()));
}

void TChatProtocolMessageHandler::handleChannelsMessage(const ::tibia::protobuf::protocol::GameserverMessageChannels& ChannelsMessage)
{
    // Id 0xffff is the own private channel still to open; the client adds NPCs and Loot.
    std::vector<TChatChannelIdentifier> channels;
    channels.reserve(static_cast<size_t>(ChannelsMessage.channels_size()) + 2);
    for (const auto& channel : ChannelsMessage.channels()) {
        const auto id = static_cast<uint16_t>(channel.id());
        const QString name = QString::fromStdString(channel.name());
        if (id == 0xffff)
            channels.push_back({ TChatChannelIdentifier::OwnPrivateChannelToOpen, id, name });
        else
            channels.push_back(TChatChannelIdentifier::channel(id, name));
    }
    channels.push_back({ TChatChannelIdentifier::Npc });
    channels.push_back({ TChatChannelIdentifier::Loot });
    Q_EMIT currentlyAvailableChannels(channels);
}

void TChatProtocolMessageHandler::handlePrivateChannelMessage(const ::tibia::protobuf::protocol::GameserverMessagePrivateChannel& PrivateChannel)
{
    m_storage.openPrivateChannel(QString::fromStdString(PrivateChannel.name()), true);
}

void TChatProtocolMessageHandler::handleChannelEventMessage(const ::tibia::protobuf::protocol::GameserverMessageChannelEvent& ChannelEvent)
{
    // Joined (0) and left (1) lines, behind consoleShowJoinLeaveMessages;
    // the invitation events add nothing.
    const uint32_t event = ChannelEvent.event_type();
    if ((event != 0 && event != 1) || !m_showJoinLeaveMessages)
        return;
    QString text = QString::fromStdString(ChannelEvent.name());
    text.append(qtTrId(event == 1 ? "chat_user_left_channel" : "chat_user_entered_channel"));
    m_storage.addChannelSystemEntryIfOpen(systemWhite(), static_cast<uint16_t>(ChannelEvent.channel_id()), text);
}

void TChatProtocolMessageHandler::handleNpcTalkPartersMessage(const ::tibia::protobuf::protocol::GameserverMessageNpcTalkParters& Message)
{
    if (Message.closed()) {
        m_chatStorage.setNpcTalkPartners({});
        m_chatStorage.setKeywordButtons({});
        return;
    }
    const std::vector<uint32_t> npcIds(Message.npc_ids().begin(), Message.npc_ids().end());
    std::vector<TKeywordButton> buttons;
    for (const auto& button : Message.buttons()) {
        // The protocol's button ids are the KEYWORDBUTTONICON_* values.
        if (button.button_id() > 9)
            throw std::runtime_error("unhandled enum value");
        buttons.push_back({ static_cast<int>(button.button_id()), QString::fromStdString(button.text()) });
    }
    m_chatStorage.setNpcTalkPartners(npcIds);
    m_chatStorage.setKeywordButtons(buttons);
}

TChatProtocolMessageHandler* TChatProtocolMessageHandler::liveInstance()
{
    return s_console;
}

bool TChatProtocolMessageHandler::isOwnName(const QString& name) const
{
    return !name.isEmpty() && name.compare(m_storage.ownName(), Qt::CaseInsensitive) == 0;
}


}
