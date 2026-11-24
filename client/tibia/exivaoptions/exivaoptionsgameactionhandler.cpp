#include "exivaoptions/exivaoptionsgameactionhandler.h"

#include "chat/chatchannel.h"
#include "exivaoptions/exivaoptionsprotocolmessagehandler.h"
#include "input/gameaction.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace tibia::exivaoptions {

namespace {

using A = input::EGameAction;
using Names = TExivaOptionsStorage::Names;

/// The names of `from` missing in `without`, in order.
std::vector<QString> difference(const Names& from, const Names& without)
{
    std::vector<QString> result;
    std::set_difference(from.begin(), from.end(), without.begin(), without.end(), std::back_inserter(result));
    return result;
}

}

TExivaOptionsGameActionHandler::TExivaOptionsGameActionHandler(std::shared_ptr<TExivaOptionsProtocolMessageHandler> protocolMessageHandler,
                                                               std::shared_ptr<TExivaOptionsStorage> storage)
    : m_protocolMessageHandler(std::move(protocolMessageHandler))
    , m_storage(std::move(storage))
{
}

void TExivaOptionsGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::EXIVA_TOGGLE_ALLOW_EVERYONE:
        toggleAllowEveryone();
        return;
    case A::EXIVA_UPADATE_OPTIONS:
        if (const auto* update = input::gameActionAs<input::TGameActionUpdateExivaOptions>(pGameAction))
            updateOptions(*update);
        return;
    case A::EXIVA_ADD_CHARACTER_TO_WHITELIST:
        if (const auto* vip = input::gameActionAs<input::TGameActionVip>(pGameAction))
            addCharacterToWhitelist(vip->vipName());
        return;
    case A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST:
        if (const auto* vip = input::gameActionAs<input::TGameActionVip>(pGameAction))
            removeCharacterFromWhitelist(vip->vipName());
        return;
    default:
        return;
    }
}

TExivaOptionsStorage::Options TExivaOptionsGameActionHandler::storedOptions() const
{
    TExivaOptionsStorage::Options options;
    options.allowAll = m_storage->allowEveryone();
    options.allowOwnGuild = m_storage->allowOwnGuild();
    options.allowOwnParty = m_storage->allowOwnParty();
    options.allowVipList = m_storage->allowVipList();
    options.allowPlayerWhitelist = m_storage->allowCharacterWhitelist();
    options.allowGuildWhitelist = m_storage->allowGuildWhitelist();
    return options;
}

void TExivaOptionsGameActionHandler::toggleAllowEveryone()
{
    TExivaOptionsStorage::Options options = storedOptions();
    options.allowAll = !options.allowAll;
    m_protocolMessageHandler->sendSetExivaOptions(options, {});
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(
        qtTrId(options.allowAll ? "exivaoptions_toggle_allow_everyone_to_true" : "exivaoptions_toggle_allow_everyone_to_false"),
        chat::systemWhite()));
}

void TExivaOptionsGameActionHandler::updateOptions(const input::TGameActionUpdateExivaOptions& action)
{
    const TExivaOptionsStorage::Whitelists whitelists = action.whitelists();
    const Names& characters = m_storage->characterWhitelist();
    const Names& guilds = m_storage->guildWhitelist();
    TExivaOptionsProtocolMessageHandler::WhitelistChanges changes;
    changes.addedCharacters = difference(whitelists.characters, characters);
    changes.removedCharacters = difference(characters, whitelists.characters);
    changes.addedGuilds = difference(whitelists.guilds, guilds);
    changes.removedGuilds = difference(guilds, whitelists.guilds);
    m_protocolMessageHandler->sendSetExivaOptions(action.options(), changes);
}

void TExivaOptionsGameActionHandler::addCharacterToWhitelist(const QString& name)
{
    TExivaOptionsProtocolMessageHandler::WhitelistChanges changes;
    changes.addedCharacters.push_back(name);
    m_protocolMessageHandler->sendSetExivaOptions(storedOptions(), changes);
}

void TExivaOptionsGameActionHandler::removeCharacterFromWhitelist(const QString& name)
{
    TExivaOptionsProtocolMessageHandler::WhitelistChanges changes;
    changes.removedCharacters.push_back(name);
    m_protocolMessageHandler->sendSetExivaOptions(storedOptions(), changes);
}

}
