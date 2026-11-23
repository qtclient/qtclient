#include "imbuing/imbuingciptypes.h"
#include "imbuing/imbuingstorage.h"
#include <utility>
#include <string>
#include <client/const.h>
#include <vector>
#include <stdexcept>
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

}

TImbuingProtocolMessageHandler::TImbuingProtocolMessageHandler(TImbuingStorage* imbuingStorage,
                                                               TImbuementDurationsStorage* imbuementDurationsStorage, QObject* parent)
    : cip::tibia::imbuing::TImbuingProtocolMessageHandler(parent)
    , m_imbuingStorage(imbuingStorage)
    , m_imbuementDurationsStorage(imbuementDurationsStorage)
{
}

void TImbuingProtocolMessageHandler::handleCloseImbuingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseImbuingDialog&)
{
    Q_EMIT requestCloseImbuingDialog();
}

void TImbuingProtocolMessageHandler::handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead&)
{
    Q_EMIT requestCloseImbuingDialog();
}

void TImbuingProtocolMessageHandler::handleImbuementDurationsMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuementDurations& ImbuementDurationsMessage)
{
    std::vector<TImbuedObjectInformation> imbuedObjects;
    imbuedObjects.reserve(static_cast<size_t>(ImbuementDurationsMessage.items_size()));
    for (const auto& entry : ImbuementDurationsMessage.items()) {
        TImbuedObjectInformation imbuedObject;
        const uint32_t inventorySlot = entry.inventory_slot();
        if (inventorySlot - 1u >= 13u)
            throw std::runtime_error("unhandled enum value");
        imbuedObject.inventorySlot = inventorySlot;
        imbuedObject.object = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(entry.item());
        for (const auto& protocolSlot : entry.slots()) {
            TImbuementSlotInformation& slot = imbuedObject.slots.emplace_back();
            if (!protocolSlot.imbued())
                continue;
            slot.imbued = true;
            slot.name = QString::fromStdString(protocolSlot.name());
            slot.iconId = protocolSlot.icon_id();
            slot.remainingMilliseconds = static_cast<qint64>(protocolSlot.duration()) * 1000;
            slot.decaying = protocolSlot.decaying();
        }
        imbuedObjects.push_back(std::move(imbuedObject));
    }
    m_imbuementDurationsStorage->setImbuedObjects(imbuedObjects);
}

void TImbuingProtocolMessageHandler::handleImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& message)
{
    // product: the pack refreshes its gold-token material status when the choice window opens.
    if (message.window_type() == tibia::client::IMBUEMENT_WINDOW_CHOICE)
        tibia::game::notify("onOpenImbuementWindow", static_cast<uint16_t>(0), static_cast<uint32_t>(0));
    TImbuingStorage* storage = m_imbuingStorage;
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
    Q_EMIT requestShowOrRefreshImbuingDialog();
}

void TImbuementDurationsStorage::setImbuedObjects(const std::vector<TImbuedObjectInformation>& ImbuedObjects)
{
    if (&m_imbuedObjects != &ImbuedObjects)
        m_imbuedObjects = ImbuedObjects;
    Q_EMIT imbuedObjectsChanged();
}

void TImbuingProtocolMessageHandler::handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message)
{
    const uint32_t type = message.dialog_type();
    if (type < 12 && (0xc0fu >> type & 1) != 0)
        Q_EMIT requestShowResultDialog(QString::fromStdString(message.message()));
}

} // namespace tibia::imbuing
