#include "imbuing/imbuingciptypes.h"
#include <utility>
#include <unordered_map>
#include <tuple>
#include <string>
#include <array>
#include <client/const.h>
#include <vector>
#include <client/staticdata.h>
#include "container/containerciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::imbuing {

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

void TImbuingProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    if (opcode == 236 && shadow->HasExtension(GameserverMessageCloseImbuingDialog::gameserver_message_close_imbuing_dialog))
        tibia::game::notify("onCloseImbuementWindow");
    if (opcode == 235 && shadow->HasExtension(GameserverMessageImbuingDialogRefresh::gameserver_message_imbuing_dialog_refresh)) {
        const auto& m = shadow->GetExtension(GameserverMessageImbuingDialogRefresh::gameserver_message_imbuing_dialog_refresh);
        static const std::array<std::string, 3> tierNames = { "Basic", "Intricate", "Powerful" };
        const auto imbuementOf = [](const ProtocolImbuement& p) {
            Imbuement imbuement;
            imbuement.id = p.imbuement_id();
            imbuement.name = p.name();
            imbuement.description = p.description();
            imbuement.tier = static_cast<uint8_t>(p.tier());
            imbuement.group = imbuement.tier < tierNames.size() ? tierNames[imbuement.tier] : std::string("Unknown");
            imbuement.imageId = static_cast<uint16_t>(p.icon_id());
            imbuement.duration = p.duration();
            imbuement.premiumOnly = false;
            for (const auto& source : p.sources())
                imbuement.sources.emplace_back(static_cast<uint16_t>(source.object_id()), static_cast<uint16_t>(source.count()), source.name());
            imbuement.cost = p.cost();
            imbuement.successRate = 100;
            imbuement.protectionCost = 0;
            return imbuement;
        };
        std::vector<Imbuement> imbuements;
        for (const auto& p : m.imbuements())
            imbuements.push_back(imbuementOf(p));
        std::vector<std::pair<uint16_t, uint16_t>> needed;
        for (const auto& n : m.needed_objects())
            needed.emplace_back(static_cast<uint16_t>(n.object_id()), static_cast<uint16_t>(n.count()));
        switch (m.window_type()) {
        case tibia::client::IMBUEMENT_WINDOW_CHOICE:
            tibia::game::notify("onOpenImbuementWindow", static_cast<uint16_t>(0), static_cast<uint32_t>(0));
            break;
        case tibia::client::IMBUEMENT_WINDOW_SCROLL:
            tibia::game::notify("onImbuementScroll", imbuements, needed);
            break;
        case tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM: {
            std::unordered_map<int, std::tuple<Imbuement, uint32_t, uint32_t>> active;
            for (int i = 0; i < m.slots_size(); ++i) {
                const auto& slot = m.slots(i);
                if (slot.occupied())
                    active[i] = std::make_tuple(imbuementOf(slot.imbuement()), slot.duration(), slot.removal_cost());
            }
            tibia::game::notify("onImbuementItem", static_cast<uint16_t>(m.object_id()), static_cast<uint8_t>(m.tier()),
                                static_cast<uint8_t>(m.slots_size()), active, imbuements, needed);
            break;
        }
        default:
            break;
        }
    }
    if (opcode == 93 && shadow->HasExtension(GameserverMessageImbuementDurations::gameserver_message_imbuement_durations)) {
        std::vector<ImbuementTrackerItem> items;
        for (const auto& entry : shadow->GetExtension(GameserverMessageImbuementDurations::gameserver_message_imbuement_durations).items()) {
            ImbuementTrackerItem& item = items.emplace_back(static_cast<uint8_t>(entry.inventory_slot()));
            const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(entry.item());
            item.typeId = slot.typeId;
            item.count = slot.stackable ? slot.count : 1;
            item.tier = slot.tier;
            item.fluidOrSubtype = slot.fluidOrSplash ? slot.fluid : 0;
            item.totalSlots = static_cast<uint8_t>(entry.slots_size());
            for (int index = 0; index < entry.slots_size(); ++index) {
                const auto& s = entry.slots(index);
                if (!s.imbued())
                    continue;
                ImbuementSlot imbued(static_cast<uint8_t>(index));
                imbued.name = s.name();
                imbued.iconId = static_cast<uint16_t>(s.icon_id());
                imbued.duration = s.duration();
                imbued.state = s.decaying();
                item.slots.emplace(static_cast<uint8_t>(index), imbued);
            }
        }
        tibia::game::notify("onUpdateImbuementTracker", items);
    }
}

} // namespace tibia::imbuing
