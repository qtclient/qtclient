#include "exivaoptions/exivaoptionsprotocolmessagehandler.h"

#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <string>
#include <utility>

namespace tibia::exivaoptions {

namespace {

TExivaOptionsStorage::Names names(const google::protobuf::RepeatedPtrField<std::string>& strings)
{
    TExivaOptionsStorage::Names result;
    for (const std::string& name : strings)
        result.insert(QString::fromStdString(name));
    return result;
}

std::vector<std::string> wire(const std::vector<QString>& names)
{
    std::vector<std::string> result;
    result.reserve(names.size());
    for (const QString& name : names)
        result.push_back(name.toStdString());
    return result;
}

}

TExivaOptionsProtocolMessageHandler::TExivaOptionsProtocolMessageHandler(std::shared_ptr<TExivaOptionsStorage> storage)
    : m_storage(std::move(storage))
{
}

void TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage(const ::tibia::protobuf::protocol::GameserverMessageExivaSuppressed&)
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::EXIVA_WAS_SUPPRESSED));
}

void TExivaOptionsProtocolMessageHandler::handleUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& message)
{
    TExivaOptionsStorage* storage = m_storage.get();
    if (!storage)
        return;
    storage->setAllowEveryone(message.allow_all());
    storage->setAllowedGroups(message.allow_own_guild(), message.allow_own_party(), message.allow_vip_list(),
                              message.allow_character_whitelist(), message.allow_guild_whitelist());
    storage->addToCharacterWhitelist(names(message.character_whitelist()));
    storage->removeFromCharacterWhitelist(names(message.removed_characters()));
    storage->addToGuildWhitelist(names(message.guild_whitelist()));
    storage->removeFromGuildWhitelist(names(message.removed_guilds()));
}

void TExivaOptionsProtocolMessageHandler::sendSetExivaOptions(const TExivaOptionsStorage::Options& options, const WhitelistChanges& changes) const
{
    tibia::game::sendExivaOptions(options.allowAll, options.allowOwnGuild, options.allowOwnParty, options.allowVipList, options.allowPlayerWhitelist,
                                  options.allowGuildWhitelist, wire(changes.addedCharacters), wire(changes.removedCharacters),
                                  wire(changes.addedGuilds), wire(changes.removedGuilds));
}

}
