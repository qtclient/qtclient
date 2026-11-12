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

using A = input::EGameAction;
using Args = std::vector<std::any>;
using WireNames = std::vector<std::string>;

constexpr A Handled[] = { A::EXIVA_TOGGLE_ALLOW_EVERYONE, A::EXIVA_ADD_CHARACTER_TO_WHITELIST, A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST,
                          A::EXIVA_UPADATE_OPTIONS };

const WireNames* namesArg(const Args& args, const size_t index) { return index < args.size() ? std::any_cast<WireNames>(&args[index]) : nullptr; }

void apply(TExivaOptionsStorage::Names& names, const WireNames* added, const WireNames* removed)
{
    if (added) {
        for (const std::string& name : *added)
            names.insert(QString::fromStdString(name));
    }
    if (removed) {
        for (const std::string& name : *removed)
            names.erase(QString::fromStdString(name));
    }
}

WireNames wire(const TExivaOptionsStorage::Names& names)
{
    WireNames result;
    result.reserve(names.size());
    for (const QString& name : names)
        result.push_back(name.toStdString());
    return result;
}

/// CIP : the names of `from` missing in `without`.
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onReceiveExivaOptions", [this](GameObject*, const Args& args) {
        m_options = { client::gamecore::argBool(args, 0), client::gamecore::argBool(args, 1), client::gamecore::argBool(args, 2),
                      client::gamecore::argBool(args, 3), client::gamecore::argBool(args, 4), client::gamecore::argBool(args, 5) };
        apply(m_characterWhitelist, namesArg(args, 6), namesArg(args, 7));
        apply(m_guildWhitelist, namesArg(args, 8), namesArg(args, 9));
        Q_EMIT optionsChanged();
        Q_EMIT allowEveryoneChanged();
        Q_EMIT allowedGroupsChanged();
        Q_EMIT characterWhitelistChanged();
        Q_EMIT guildWhitelistChanged();
        return std::any{};
    });
    // CIP TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage : ExivaSuppressed (0xC9, no fields).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onExivaSuppressed", [](GameObject*, const Args&) {
        input::publish(input::makeAction(A::EXIVA_WAS_SUPPRESSED));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_options = {};
        m_characterWhitelist.clear();
        m_guildWhitelist.clear();
        Q_EMIT optionsChanged();
        Q_EMIT allowEveryoneChanged();
        Q_EMIT allowedGroupsChanged();
        Q_EMIT characterWhitelistChanged();
        Q_EMIT guildWhitelistChanged();
    });

    for (const A type : Handled) {
        input::setActionHandler(type, [this](const input::TGameAction& action) {
            switch (action.type) {
            case A::EXIVA_TOGGLE_ALLOW_EVERYONE:
                toggleAllowEveryone();
                return true;
            case A::EXIVA_ADD_CHARACTER_TO_WHITELIST:
            case A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST:
                requestWhitelistChange(action.text, action.type == A::EXIVA_ADD_CHARACTER_TO_WHITELIST);
                return true;
            case A::EXIVA_UPADATE_OPTIONS:
                // Dialog OK: re-send current options vs stored whitelists (empty diffs
                // when unchanged). Full option payloads ride on CIP's typed action.
                update(m_options, m_characterWhitelist, m_guildWhitelist);
                return true;
            default:
                return false;
            }
        });
    }
}

TExivaOptionsStorage::~TExivaOptionsStorage()
{
    for (const A type : Handled)
        input::setActionHandler(type, nullptr);
}

void TExivaOptionsStorage::send(const Options& options, const Names& addCharacters, const Names& removeCharacters, const Names& addGuilds,
                                const Names& removeGuilds) const
{
    tibia::game::sendExivaOptions(options.allowAll, options.allowOwnGuild, options.allowOwnParty, options.allowVipList, options.allowPlayerWhitelist,
                            options.allowGuildWhitelist, wire(addCharacters), wire(removeCharacters), wire(addGuilds), wire(removeGuilds));
}

// b1/35f9fbe2b41d6fe2
void TExivaOptionsStorage::toggleAllowEveryone() const
{
    Options options = m_options;
    options.allowAll = !options.allowAll;
    send(options, {}, {}, {}, {});
    auto status = input::mutableClone(*input::makeAction(A::SET_STATUS_LINE));
    status->text = qtTrId(options.allowAll ? "exivaoptions_toggle_allow_everyone_to_true" : "exivaoptions_toggle_allow_everyone_to_false");
    status->color = chat::systemWhite();
    input::publish(status);
}

// b1/ca2e14af23614ecb
void TExivaOptionsStorage::requestWhitelistChange(const QString& name, const bool add) const
{
    if (name.isEmpty())
        return;
    const Names names{ name };
    send(m_options, add ? names : Names{}, add ? Names{} : names, {}, {});
}

// b1/08eb6e5ff5a60fa5
void TExivaOptionsStorage::update(const Options& options, const Names& characters, const Names& guilds) const
{
    send(options, difference(characters, m_characterWhitelist), difference(m_characterWhitelist, characters), difference(guilds, m_guildWhitelist),
         difference(m_guildWhitelist, guilds));
}

}
