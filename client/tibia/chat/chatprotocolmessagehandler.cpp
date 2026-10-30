#include "chatprotocolmessagehandler.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "config/clientoptiondefaults.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "shared/tibiaurltools.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/protocolcodes.h>

#include <memory>

#include <QJsonArray>

#include <algorithm>
#include <tuple>

namespace tibia::chat {

namespace {

using client::gamecore::argInt;
using client::gamecore::argString;
using Args = std::vector<std::any>;

/// CIP's message modes: the protocol's (identity table 0x102309ad4).
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
    Report = 38,
    HotkeyUse = 39,
    Mana = 43,
    BeyondLast = 44,
    Attention = 48,
    BoostedCreature = 49,
    OfflineTraining = 50,
    Transaction = 51,
    Mode54 = 54,
};

int protocolMode(const Args& args, const size_t index)
{
    if (index < args.size())
        if (const auto* mode = std::any_cast<Otc::MessageMode>(&args[index]))
            return Proto::translateMessageModeToServer(*mode);
    return -1;
}

/// Missing key → CIP default from FUN_1009ad850 (option id table; 0x1a
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
    : QObject(parent)
{
    const QJsonObject options = whiteBlacklistOptions();
    const auto load = [](const QJsonArray& names, Entries& entries) {
        for (const QJsonValue& name : names)
            if (!name.toString().isEmpty())
                insert(entries, { name.toString(), true });
    };
    load(options.value(QLatin1String("blacklistedNames")).toArray(), m_blacklist);
    load(options.value(QLatin1String("whitelistedNames")).toArray(), m_whitelist);
}

bool TWhiteBlacklist::contains(const Entries& entries, const QString& name)
{
    return std::any_of(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.name.compare(name, Qt::CaseInsensitive) == 0; });
}

void TWhiteBlacklist::insert(Entries& entries, const Entry& entry)
{
    if (entry.name.isEmpty() || contains(entries, entry.name))
        return;
    const auto at = std::lower_bound(entries.begin(), entries.end(), entry,
                                     [](const Entry& a, const Entry& b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
    entries.insert(at, entry);
}

bool TWhiteBlacklist::isOnBlacklist(const QString& name) const
{
    return !name.isEmpty() && contains(m_blacklist, name);
}

bool TWhiteBlacklist::isWhitelisted(const QString& name) const
{
    if (!m_whitelistActive)
        return false;
    return contains(m_whitelist, name) || (m_allowVips && m_isVip && m_isVip(name));
}

bool TWhiteBlacklist::isBlacklisted(const QString& name) const
{
    return m_blacklistActive && !isWhitelisted(name) && isOnBlacklist(name);
}

bool TWhiteBlacklist::filtersYell(const QString& name) const
{
    return m_blacklistActive && m_ignoreYelling && !isWhitelisted(name);
}

bool TWhiteBlacklist::filtersPrivateMessage(const QString& name) const
{
    return m_blacklistActive && m_ignorePrivateMessages && !isWhitelisted(name);
}

bool TWhiteBlacklist::isExemptMode(const int mode)
{
    return mode >= 0 && mode < 0x36 && ((0x2000300000ec00ULL >> mode) & 1) != 0;
}

void TWhiteBlacklist::ignore(const QString& name)
{
    if (name.isEmpty() || isOnBlacklist(name))
        return;
    insert(m_blacklist, { name, false });
    save();
}

void TWhiteBlacklist::unignore(const QString& name)
{
    const auto removed = std::erase_if(m_blacklist, [&](const Entry& entry) { return entry.name.compare(name, Qt::CaseInsensitive) == 0; });
    if (removed == 0)
        return;
    save();
}

void TWhiteBlacklist::setLists(const Entries& whitelist, const Entries& blacklist)
{
    m_whitelist.clear();
    for (const Entry& entry : whitelist)
        insert(m_whitelist, entry);
    m_blacklist.clear();
    for (const Entry& entry : blacklist)
        insert(m_blacklist, entry);
    save();
}

void TWhiteBlacklist::setFlags(const bool blacklistActive, const bool whitelistActive, const bool ignoreYelling,
                               const bool ignorePrivateMessages, const bool allowVips)
{
    m_blacklistActive = blacklistActive;
    m_whitelistActive = whitelistActive;
    m_ignoreYelling = ignoreYelling;
    m_ignorePrivateMessages = ignorePrivateMessages;
    m_allowVips = allowVips;
}

void TWhiteBlacklist::save()
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
    Q_EMIT changed();
}

TChatProtocolMessageHandler::TChatProtocolMessageHandler(TChatChannelStorage& storage, TWhiteBlacklist& blacklist, QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_blacklist(blacklist)
{
    m_hooks.emplace_back("g_game", "onTalk", [this](LuaObject*, const Args& args) {
        onTalk(argString(args, 0), static_cast<int>(argInt(args, 1)), protocolMode(args, 2),
               shared::TTibiaUrlTools::process(argString(args, 3)),
               static_cast<uint16_t>(argInt(args, 4)), static_cast<uint32_t>(argInt(args, 6)));
        return std::any();
    });
    m_hooks.emplace_back("g_game", "onTextMessage", [this](LuaObject*, const Args& args) {
        onTextMessage(protocolMode(args, 0), shared::TTibiaUrlTools::process(argString(args, 1)));
        return std::any();
    });
    m_hooks.emplace_back("g_game", "onChannelTextMessage", [this](LuaObject*, const Args& args) {
        onChannelTextMessage(protocolMode(args, 0), static_cast<uint16_t>(argInt(args, 1)),
                             shared::TTibiaUrlTools::process(argString(args, 2)));
        return std::any();
    });
    // FUN_100998810: GameServerChannels.
    m_hooks.emplace_back("g_game", "onChannelList", [this](LuaObject*, const Args& args) {
        std::vector<std::pair<uint16_t, QString>> channels;
        if (!args.empty()) {
            if (const auto* list = std::any_cast<std::vector<std::tuple<uint16_t, std::string>>>(&args[0])) {
                for (const auto& [id, name] : *list)
                    channels.emplace_back(id, QString::fromStdString(name));
            }
        }
        Q_EMIT channelListReceived(channels);
        return std::any();
    });
    // FUN_1009983c0: GameServerOpenChannel.
    m_hooks.emplace_back("g_game", "onOpenChannel", [this](LuaObject*, const Args& args) {
        const auto id = static_cast<uint16_t>(argInt(args, 0));
        m_storage.open(TChatChannelIdentifier::channel(id, argString(args, 1)), m_foregroundChannels.erase(id) > 0);
        return std::any();
    });
    // FUN_1009985d0: GameServerOpenOwnChannel.
    m_hooks.emplace_back("g_game", "onOpenOwnPrivateChannel", [this](LuaObject*, const Args& args) {
        const bool requested = m_foregroundChannels.erase(OwnChannelRequest) > 0;
        TChatChannel* channel = m_storage.open(TChatChannelIdentifier::channel(static_cast<uint16_t>(argInt(args, 0)), argString(args, 1)), requested);
        channel->setOwnChannel(true);
        Q_EMIT channel->propertiesChanged();
        return std::any();
    });
    // FUN_100998db0: GameServerPrivateChannel.
    m_hooks.emplace_back("g_game", "onOpenPrivateChannel", [this](LuaObject*, const Args& args) {
        m_storage.open(TChatChannelIdentifier::messageTo(argString(args, 0)), true);
        return std::any();
    });
    m_hooks.emplace_back("g_game", "onCloseChannel", [this](LuaObject*, const Args& args) {
        onCloseChannel(static_cast<uint16_t>(argInt(args, 0)));
        return std::any();
    });
    m_hooks.emplace_back("g_game", "onChannelEvent", [this](LuaObject*, const Args& args) {
        onChannelEvent(static_cast<uint16_t>(argInt(args, 0)), argString(args, 1), static_cast<int>(argInt(args, 2)));
        return std::any();
    });
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
    m_chatSoundsEnabled = option(options, "ChatEnabled");
    m_partyMessagesEnabled = option(options, "PartyMessagesEnabled");
    m_guildMessagesEnabled = option(options, "GuildMessagesEnabled");
    m_privateMessagesEnabled = option(options, "PrivateMessagesEnabled");
    m_npcMessagesEnabled = option(options, "NpcMessageseEnabled");
    m_globalMessagesEnabled = option(options, "GlobalMessagesEnabled");
    m_teamFinderMessagesEnabled = option(options, "TeamFinderMessagesEnabled");
    m_raidMessagesEnabled = option(options, "RaidMessagesEnabled");
    m_systemMessagesEnabled = option(options, "SystemMessagesEnabled");
}

bool TChatProtocolMessageHandler::consoleMessageAllowed(const int mode, const uint16_t channelId) const
{
    const TChatChannelIdentifier id = TChatChannelIdentifier::channel(channelId);
    switch (mode) {
    case Party:
    case PartyManagement:
        return m_partyMessagesEnabled;
    case Guild:
        return m_guildMessagesEnabled;
    case PrivateFrom:
    case GamemasterPrivateFrom:
        return m_privateMessagesEnabled;
    case NpcFromStartBlock:
    case NpcFrom:
    case NpcTo:
        return m_npcMessagesEnabled;
    case GamemasterBroadcast:
    case GamemasterChannel:
        return m_globalMessagesEnabled;
    case Attention:
        return m_raidMessagesEnabled;
    case Channel:
    case ChannelHighlight:
        if (id.isPartyChannel())
            return m_partyMessagesEnabled;
        if (id.isGuildChannel())
            return m_guildMessagesEnabled;
        // Team Finder is CIP channel 5.
        if (channelId == 5)
            return m_teamFinderMessagesEnabled;
        return true;
    case ChannelManagement:
    case Admin:
    case GameHighlight:
    case Report:
    case Failure:
        return m_systemMessagesEnabled;
    default:
        return true;
    }
}

bool TChatProtocolMessageHandler::chatArrivedSoundAllowed(const int mode, const uint16_t channelId) const
{
    return m_chatSoundsEnabled && consoleMessageAllowed(mode, channelId);
}

bool TChatProtocolMessageHandler::isOwnName(const QString& name) const
{
    return !name.isEmpty() && name.compare(m_storage.ownName(), Qt::CaseInsensitive) == 0;
}

void TChatProtocolMessageHandler::onTalk(const QString& name, const int level, const int mode, const QString& text, const uint16_t channelId,
                                         const uint32_t statementId)
{
    // FUN_100996120: the blacklist (vslot 0xa8) unless the mode is exempt (vslot 0xd0), then yells (0xb0) and private messages (0xb8).
    const bool filtered = !TWhiteBlacklist::isExemptMode(mode)
                          && (m_blacklist.isBlacklisted(name) || (mode == Yell && m_blacklist.filtersYell(name))
                              || (mode == PrivateFrom && m_blacklist.filtersPrivateMessage(name)));
    if (filtered) {
        Q_EMIT messageFiltered();
        return;
    }
    if (!consoleMessageAllowed(mode, channelId))
        return;
    const QColor color = consoleColor(mode);
    bool arrivedSound = false;
    switch (mode) {
    case Say:
    case Whisper:
        m_storage.addLocalEntry(color, text, name, level, statementId);
        break;
    case Yell:
        // FUN_1009971a0: yelled text is upper case.
        m_storage.addLocalEntry(color, text.toUpper(), name, level, statementId);
        break;
    case PrivateFrom:
        addPrivateMessage(name, level, mode, text, statementId);
        arrivedSound = !isOwnName(name);
        break;
    case Channel:
    case ChannelHighlight:
    case GamemasterChannel:
        m_storage.addChannelEntry(channelId, color, text, name, level, statementId);
        arrivedSound = !isOwnName(name);
        break;
    case Spell:
        if (isOwnName(name) ? m_showOwnSpells : m_showOthersSpells)
            m_storage.addLocalEntry(color, text, name, level, statementId);
        break;
    case NpcFromStartBlock:
    case NpcFrom:
    case NpcTo:
        // Player lines to an NPC (mode 12) are not echoed by a typed dialog;
        // quest expeditions send them, and they belong on the NPCs tab with
        // the NPC's replies. The dialog pane shows this channel.
        m_storage.addNpcEntry(color, text, name);
        arrivedSound = true;
        break;
    case GamemasterBroadcast:
    case GamemasterPrivateFrom:
        m_storage.addBroadcastEntry(color, text, name, level, statementId);
        arrivedSound = !isOwnName(name);
        break;
    case Game:
        m_storage.addServerLogEntry(color, text);
        break;
    default:
        break;
    }
    // CIP 0x10098df40 / 0x10098e560 / 0x10098eb80: SOUND_TRIGGER 0xade (2782).
    if (arrivedSound && chatArrivedSoundAllowed(mode, channelId)) {
        auto action = input::mutableClone(*input::makeAction(input::EGameAction::SOUND_TRIGGER));
        action->parameter = cip::tibia::qmlcomponents::TSoundHelperService::CHAT_MESSAGE_ARRIVED;
        input::publish(action);
    }
}

void TChatProtocolMessageHandler::addPrivateMessage(const QString& name, const int level, const int mode, const QString& text,
                                                    const uint32_t statementId)
{
    // FUN_100996d10: if *(this+0x8f) (openNewPrivateMessagesInNewTab) and the
    // sender is not own and no messageTo tab exists, open one (makeCurrent=0).
    if (m_openPrivateMessagesInNewTab && !isOwnName(name) && !m_storage.find(TChatChannelIdentifier::messageTo(name))) {
        m_storage.open(TChatChannelIdentifier::messageTo(name), false);
        Q_EMIT privateChannelOpened(name);
    }
    m_storage.addPrivateEntry(name, consoleColor(mode), text, name, level, statementId);
}

void TChatProtocolMessageHandler::onTextMessage(const int mode, const QString& text)
{
    if (!consoleMessageAllowed(mode, 0))
        return;
    bool log = false;
    switch (mode) {
    case Login:
    case Game:
        log = m_showEventMessages && m_systemMessagesEnabled;
        break;
    case Admin:
    case GameHighlight:
    case Report:
    case BeyondLast:
    case BoostedCreature:
    case OfflineTraining:
    case Transaction:
        log = m_systemMessagesEnabled;
        break;
    case Attention:
        log = m_raidMessagesEnabled;
        break;
    case Look:
    case TradeNpc:
    case HotkeyUse:
        log = m_showInfoMessages;
        break;
    case DamageDealt:
    case DamageReceived:
    case Heal:
    case Experience:
    case Status:
    case Mana:
        log = m_showStatusMessagesOwn;
        break;
    case DamageOthers:
    case HealOthers:
    case ExperienceOthers:
        log = m_showStatusMessagesOfOthers;
        break;
    case Loot:
        // FUN_100997ed0.
        m_storage.addLootEntry(consoleColor(mode), text);
        break;
    default:
        break;
    }
    if (log)
        m_storage.addServerLogEntry(consoleColor(mode), text);
}

void TChatProtocolMessageHandler::onChannelTextMessage(const int mode, const uint16_t channelId, const QString& text)
{
    if (mode == ChannelManagement || mode == Guild || mode == PartyManagement || mode == Party) {
        if (!consoleMessageAllowed(mode, channelId))
            return;
        m_storage.addChannelSystemEntry(channelId, consoleColor(mode), text);
    }
}

void TChatProtocolMessageHandler::onChannelEvent(const uint16_t channelId, const QString& name, const int type)
{
    if (!m_showJoinLeaveMessages || (type != 0 && type != 1))
        return;
    const QString text = name + qtTrId(type == 1 ? "chat_user_left_channel" : "chat_user_entered_channel");
    m_storage.addChannelSystemEntry(channelId, systemWhite(), text);
}

void TChatProtocolMessageHandler::onCloseChannel(const uint16_t channelId)
{
    TChatChannel* channel = m_storage.findChannelId(channelId);
    if (!channel)
        return;
    const char* id = channel->identifier().isPartyChannel() ? "chat_team_finder_channel_closed_message" : "chat_channel_closed_message";
    m_storage.addSystemEntry(channel, systemRed(), qtTrId(id));
    channel->setClosedByServer(true);
    Q_EMIT channel->propertiesChanged();
}

}
