#include "quickloot/quicklootciptypes.h"
#include <vector>
#include <tuple>
#include <protocol.pb.h>
#include <tibia/game/gamenotify.h>
#include "protocol/protocolciptypes.h"

namespace tibia::quickloot {

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

void TManagedContainersProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/,
                                                                       const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow || opcode != 192
        || !shadow->HasExtension(GameserverMessageUpdateManagedContainers::gameserver_message_update_managed_containers))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageUpdateManagedContainers::gameserver_message_update_managed_containers);
    std::vector<std::tuple<uint8_t, uint16_t, uint16_t>> containers;
    for (const auto& c : m.containers())
        containers.emplace_back(static_cast<uint8_t>(c.category()), static_cast<uint16_t>(c.loot_container_id()),
                                static_cast<uint16_t>(c.obtainer_container_id()));
    tibia::game::notify("onQuickLootContainers", m.fallback_to_main_container(), containers);
}

} // namespace tibia::quickloot
