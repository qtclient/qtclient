#include "exivaoptionsstorage.h"

#include "chat/chatchannel.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::exivaoptions {

namespace {

TExivaOptionsStorage* g_instance = nullptr;

using A = input::EGameAction;
using WireNames = std::vector<std::string>;

constexpr A Handled[] = { A::EXIVA_TOGGLE_ALLOW_EVERYONE, A::EXIVA_ADD_CHARACTER_TO_WHITELIST, A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST,
                          A::EXIVA_UPADATE_OPTIONS };

WireNames wire(const TExivaOptionsStorage::Names& names)
{
    WireNames result;
    result.reserve(names.size());
    for (const QString& name : names)
        result.push_back(name.toStdString());
    return result;
}

/// The names of `from` missing in `without`.
TExivaOptionsStorage::Names difference(const TExivaOptionsStorage::Names& from, const TExivaOptionsStorage::Names& without)
{
    TExivaOptionsStorage::Names result;
    std::set_difference(from.begin(), from.end(), without.begin(), without.end(), std::inserter(result, result.end()));
    return result;
}

}

TExivaOptionsStorage::TExivaOptionsStorage(QObject* parent)
    : QObject(parent)
{
    g_instance = this;
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_options = {};
        m_characterWhitelist.clear();
        m_guildWhitelist.clear();
        Q_EMIT allowEveryoneChanged();
        Q_EMIT allowedGroupsChanged();
        Q_EMIT characterWhitelistChanged();
        Q_EMIT guildWhitelistChanged();
    });

    for (const A type : Handled)
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

TExivaOptionsStorage::~TExivaOptionsStorage()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TExivaOptionsStorage* TExivaOptionsStorage::instance() { return g_instance; }

bool TExivaOptionsStorage::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::EXIVA_TOGGLE_ALLOW_EVERYONE:
        toggleAllowEveryone();
        return true;
    case A::EXIVA_ADD_CHARACTER_TO_WHITELIST:
    case A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST:
        if (const auto* vip = input::gameActionAs<input::TGameActionVip>(&action))
            requestWhitelistChange(vip->vipName(), action.gameActionType() == A::EXIVA_ADD_CHARACTER_TO_WHITELIST);
        return true;
    case A::EXIVA_UPADATE_OPTIONS:
        // The dialog's apply calls update() with its payload; this action
        // type carries none here, so the stored options go out unchanged.
        update(m_options, m_characterWhitelist, m_guildWhitelist);
        return true;
    default:
        return false;
    }
}

void TExivaOptionsStorage::setAllowEveryone(const bool allow)
{
    if (m_options.allowAll == allow)
        return;
    m_options.allowAll = allow;
    Q_EMIT allowEveryoneChanged();
}

void TExivaOptionsStorage::setAllowedGroups(const bool ownGuild, const bool ownParty, const bool vipList, const bool characterWhitelist,
                                            const bool guildWhitelist)
{
    if (m_options.allowOwnGuild == ownGuild && m_options.allowOwnParty == ownParty && m_options.allowVipList == vipList
        && m_options.allowPlayerWhitelist == characterWhitelist && m_options.allowGuildWhitelist == guildWhitelist)
        return;
    m_options.allowOwnGuild = ownGuild;
    m_options.allowOwnParty = ownParty;
    m_options.allowVipList = vipList;
    m_options.allowPlayerWhitelist = characterWhitelist;
    m_options.allowGuildWhitelist = guildWhitelist;
    Q_EMIT allowedGroupsChanged();
}

void TExivaOptionsStorage::addToCharacterWhitelist(const Names& names)
{
    const size_t before = m_characterWhitelist.size();
    m_characterWhitelist.insert(names.begin(), names.end());
    if (before != m_characterWhitelist.size())
        Q_EMIT characterWhitelistChanged();
}

void TExivaOptionsStorage::removeFromCharacterWhitelist(const Names& names)
{
    if (names.empty())
        return;
    const size_t before = m_characterWhitelist.size();
    for (const QString& name : names)
        m_characterWhitelist.erase(name);
    if (before != m_characterWhitelist.size())
        Q_EMIT characterWhitelistChanged();
}

void TExivaOptionsStorage::addToGuildWhitelist(const Names& names)
{
    const size_t before = m_guildWhitelist.size();
    m_guildWhitelist.insert(names.begin(), names.end());
    if (before != m_guildWhitelist.size())
        Q_EMIT guildWhitelistChanged();
}

void TExivaOptionsStorage::removeFromGuildWhitelist(const Names& names)
{
    if (names.empty())
        return;
    const size_t before = m_guildWhitelist.size();
    for (const QString& name : names)
        m_guildWhitelist.erase(name);
    if (before != m_guildWhitelist.size())
        Q_EMIT guildWhitelistChanged();
}

void TExivaOptionsStorage::send(const Options& options, const Names& addCharacters, const Names& removeCharacters, const Names& addGuilds,
                                const Names& removeGuilds) const
{
    tibia::game::sendExivaOptions(options.allowAll, options.allowOwnGuild, options.allowOwnParty, options.allowVipList, options.allowPlayerWhitelist,
                            options.allowGuildWhitelist, wire(addCharacters), wire(removeCharacters), wire(addGuilds), wire(removeGuilds));
}

void TExivaOptionsStorage::toggleAllowEveryone()
{
    Options options = m_options;
    options.allowAll = !options.allowAll;
    send(options, {}, {}, {}, {});
    auto status = std::make_shared<input::TGameActionSetStatusLine>(qtTrId(options.allowAll ? "exivaoptions_toggle_allow_everyone_to_true" : "exivaoptions_toggle_allow_everyone_to_false"), chat::systemWhite());
    Q_EMIT publishGameAction(status);
}

void TExivaOptionsStorage::requestWhitelistChange(const QString& name, const bool add) const
{
    const Names names{ name };
    send(m_options, add ? names : Names{}, add ? Names{} : names, {}, {});
}

void TExivaOptionsStorage::update(const Options& options, const Names& characters, const Names& guilds) const
{
    send(options, difference(characters, m_characterWhitelist), difference(m_characterWhitelist, characters), difference(guilds, m_guildWhitelist),
         difference(m_guildWhitelist, guilds));
}

}
