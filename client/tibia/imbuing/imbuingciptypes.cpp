#include "imbuing/imbuingciptypes.h"
#include "imbuing/imbuingstorage.h"
#include <utility>
#include <string>
#include <client/const.h>
#include <vector>
#include <client/staticdata.h>
#include "container/containerciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::imbuing {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

namespace {

/// One imbuement as the dialog refresh sends it.
TImbuement imbuementFromProtocol(const ::tibia::protobuf::protocol::ProtocolImbuement& protocolImbuement)
{
    TImbuement imbuement;
    imbuement.id = protocolImbuement.imbuement_id();
    imbuement.name = QString::fromStdString(protocolImbuement.name());
    imbuement.description = QString::fromStdString(protocolImbuement.description());
    imbuement.tier = protocolImbuement.tier();
    imbuement.iconId = protocolImbuement.icon_id();
    imbuement.duration = protocolImbuement.duration();
    imbuement.goldCost = protocolImbuement.cost();
    imbuement.astralSources.reserve(static_cast<size_t>(protocolImbuement.sources_size()));
    for (const auto& source : protocolImbuement.sources())
        imbuement.astralSources.push_back({ static_cast<uint16_t>(source.object_id()), source.count(), QString::fromStdString(source.name()) });
    return imbuement;
}

/// The whole refresh goes to the storage; the dialog then shows or updates.
void handleImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& message)
{
    TImbuingStorage* storage = TImbuingStorage::instance();
    if (!storage)
        return;
    std::vector<TImbuementSlot> slots;
    slots.reserve(static_cast<size_t>(message.slots_size()));
    for (int index = 0; index < message.slots_size(); ++index) {
        const auto& protocolSlot = message.slots(index);
        TImbuementSlot& slot = slots.emplace_back();
        slot.index = static_cast<uint32_t>(index);
        if (!protocolSlot.has_imbuement())
            continue;
        slot.imbuement = imbuementFromProtocol(protocolSlot.imbuement());
        slot.remainingDuration = protocolSlot.duration();
        slot.clearingGoldCost = protocolSlot.removal_cost();
        slot.isEmpty = false;
    }
    std::vector<TImbuement> imbuements;
    imbuements.reserve(static_cast<size_t>(message.imbuements_size()));
    for (const auto& protocolImbuement : message.imbuements())
        imbuements.push_back(imbuementFromProtocol(protocolImbuement));
    std::vector<TImbuementSource> ownedObjects;
    ownedObjects.reserve(static_cast<size_t>(message.needed_objects_size()));
    for (const auto& owned : message.needed_objects())
        ownedObjects.push_back({ static_cast<uint16_t>(owned.object_id()), owned.count(), {} });
    std::optional<uint8_t> tier;
    if (message.has_tier())
        tier = static_cast<uint8_t>(message.tier());
    storage->setImbuingData(message.window_type(), message.flag(), static_cast<uint16_t>(message.object_id()), tier, std::move(slots),
                            std::move(imbuements), std::move(ownedObjects));
    Q_EMIT storage->requestShowOrRefreshImbuingDialog();
}

/// Only the dialog types that report an imbuing result (mask 0xc0f).
void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message)
{
    TImbuingStorage* storage = TImbuingStorage::instance();
    const uint32_t type = message.dialog_type();
    if (storage && type < 12 && (0xc0fu >> type & 1) != 0)
        Q_EMIT storage->requestShowResultDialog(QString::fromStdString(message.message()));
}

}

void TImbuingProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    if (opcode == 236 && shadow->HasExtension(GameserverMessageCloseImbuingDialog::gameserver_message_close_imbuing_dialog)) {
        if (TImbuingStorage* storage = TImbuingStorage::instance())
            Q_EMIT storage->requestCloseImbuingDialog();
    }
    if (opcode == 237 && shadow->HasExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog))
        handleShowMessageDialogMessage(shadow->GetExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog));
    if (opcode == 235 && shadow->HasExtension(GameserverMessageImbuingDialogRefresh::gameserver_message_imbuing_dialog_refresh)) {
        const auto& message = shadow->GetExtension(GameserverMessageImbuingDialogRefresh::gameserver_message_imbuing_dialog_refresh);
        // product: the pack refreshes its gold-token material status when the choice window opens.
        if (message.window_type() == tibia::client::IMBUEMENT_WINDOW_CHOICE)
            tibia::game::notify("onOpenImbuementWindow", static_cast<uint16_t>(0), static_cast<uint32_t>(0));
        handleImbuingDialogRefreshMessage(message);
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
