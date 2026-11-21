#include "exivaoptions/exivaoptionsciptypes.h"
#include <vector>
#include <string>

#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"

#include <protocol.pb.h>

namespace tibia::exivaoptions {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

namespace {

TExivaOptionsStorage::Names names(const google::protobuf::RepeatedPtrField<std::string>& strings)
{
    TExivaOptionsStorage::Names result;
    for (const std::string& name : strings)
        result.insert(QString::fromStdString(name));
    return result;
}

}

TExivaOptionsProtocolMessageHandler::TExivaOptionsProtocolMessageHandler(TExivaOptionsStorage* storage, QObject* parent)
    : cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

void TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage(const ::tibia::protobuf::protocol::GameserverMessageExivaSuppressed&)
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::EXIVA_WAS_SUPPRESSED));
}

void TExivaOptionsProtocolMessageHandler::handleUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& message)
{
    TExivaOptionsStorage* storage = m_storage;
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

} // namespace tibia::exivaoptions
