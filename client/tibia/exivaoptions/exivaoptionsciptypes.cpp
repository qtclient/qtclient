#include "exivaoptions/exivaoptionsciptypes.h"
#include <vector>
#include <string>

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

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

void TExivaOptionsProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    if (opcode == 201 && shadow->HasExtension(GameserverMessageExivaSuppressed::gameserver_message_exiva_suppressed))
        tibia::game::notify("onExivaSuppressed");
    if (opcode == 202 && shadow->HasExtension(GameserverMessageUpdateExivaOptions::gameserver_message_update_exiva_options)) {
        const auto& m = shadow->GetExtension(GameserverMessageUpdateExivaOptions::gameserver_message_update_exiva_options);
        const auto list = [](const auto& strings) { return std::vector<std::string>(strings.begin(), strings.end()); };
        tibia::game::notify("onReceiveExivaOptions", m.allow_all(), m.allow_own_guild(), m.allow_own_party(), m.allow_vip_list(),
                            m.allow_character_whitelist(), m.allow_guild_whitelist(), list(m.character_whitelist()),
                            list(m.removed_characters()), list(m.guild_whitelist()), list(m.removed_guilds()));
    }
}

} // namespace tibia::exivaoptions
