#include "exivaoptions/exivaoptionsciptypes.h"
#include <vector>
#include <string>

#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"


#include <protocol.pb.h>

namespace tibia::exivaoptions {

namespace {
} // namespace

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

/// The flags replace the stored ones; the listed names join or leave the whitelists.
void handleUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& message)
{
    TExivaOptionsStorage* storage = TExivaOptionsStorage::instance();
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

/// A suppressed exiva only flashes the chat's exiva button.
void handleExivaSuppressedMessage() { input::publish(input::makeAction(input::EGameAction::EXIVA_WAS_SUPPRESSED)); }

}

void TExivaOptionsProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    if (opcode == 201 && shadow->HasExtension(GameserverMessageExivaSuppressed::gameserver_message_exiva_suppressed))
        handleExivaSuppressedMessage();
    if (opcode == 202 && shadow->HasExtension(GameserverMessageUpdateExivaOptions::gameserver_message_update_exiva_options))
        handleUpdateExivaOptionsMessage(shadow->GetExtension(GameserverMessageUpdateExivaOptions::gameserver_message_update_exiva_options));
}

} // namespace tibia::exivaoptions
