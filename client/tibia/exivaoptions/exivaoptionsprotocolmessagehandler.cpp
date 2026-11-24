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

void addNames(google::protobuf::RepeatedPtrField<std::string>* field, const std::vector<QString>& names)
{
    for (const QString& name : names)
        field->Add(name.toStdString());
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

void TExivaOptionsProtocolMessageHandler::setExivaOptions(const TExivaOptionsStorage::Options& options, const WhitelistChanges& changes)
{
    ::tibia::protobuf::protocol::GameclientMessageUpdateExivaOptions message;
    message.set_allow_all(options.allowAll);
    message.set_allow_own_guild(options.allowOwnGuild);
    message.set_allow_own_party(options.allowOwnParty);
    message.set_allow_vip_list(options.allowVipList);
    message.set_allow_player_whitelist(options.allowPlayerWhitelist);
    message.set_allow_guild_whitelist(options.allowGuildWhitelist);
    addNames(message.mutable_add_characters(), changes.addedCharacters);
    addNames(message.mutable_remove_characters(), changes.removedCharacters);
    addNames(message.mutable_add_guilds(), changes.addedGuilds);
    addNames(message.mutable_remove_guilds(), changes.removedGuilds);
    Q_EMIT sendSetExivaOptions(message);
}

}
