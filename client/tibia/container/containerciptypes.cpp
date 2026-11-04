#include "container/containerciptypes.h"

#include "appearances/appearanceinstance.h"
#include "protocol/protocolciptypes.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "tibia/engine/client/outfit.h"
#include "tibia/game/gamenotify.h"
#include "tibia/game/gamesession.h"
#include <tibia/game/gamesession.h>
#include "tibia/world/localplayer.h"

#include <client/item.h>
#include <framework/core/clock.h>
#include <framework/core/logger.h>

#include <protocol.pb.h>

#include <algorithm>
#include <tibia/game/thingsession_forward.h>

namespace tibia::container {

namespace {
struct CipTypedKeep {
    TContainer tContainer;
    TContainerGameActionProvider tContainerGameActionProvider;
    TContainerRenderProvider tContainerRenderProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

TContainerStorage* g_containerStorage = nullptr;
TInventoryContainer* g_inventoryContainer = nullptr;
TContainerProtocolMessageHandler* g_liveContainerProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageCloseContainer;
using ::tibia::protobuf::protocol::GameserverMessageDeleteInventory;
using ::tibia::protobuf::protocol::GameserverMessageSetInventory;
using ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable;
using ::tibia::protobuf::protocol::ProtocolInventoryItem;
} // namespace

TContainerProtocolMessageHandler::TContainerProtocolMessageHandler(QObject* parent)
    : cip::tibia::container::TContainerProtocolMessageHandler(parent)
{
}

TContainerProtocolMessageHandler* TContainerProtocolMessageHandler::liveInstance()
{
    return g_liveContainerProtocolHandler;
}

void TContainerProtocolMessageHandler::setLiveInstance(TContainerProtocolMessageHandler* handler)
{
    g_liveContainerProtocolHandler = handler;
}

void TContainerProtocolMessageHandler::handleCloseContainerMessage(
    const GameserverMessageCloseContainer& CloseContainerMessage)
{
    // Same side effects as Game::processCloseContainer / parseCloseContainer.
    // processCloseContainer is ProtocolGame-friend protected — apply via storage.
    const auto containerId = static_cast<TContainerID>(CloseContainerMessage.container_id());
    if (TContainerStorage* storage = TContainerStorage::instance())
        storage->closeContainer(containerId);
}

void TContainerProtocolMessageHandler::handleSetInventoryMessage(
    const GameserverMessageSetInventory& SetInventoryMessage)
{
    // Same side effects as Game::processInventoryChange / parseAddInventoryItem.
    const auto slot = static_cast<uint8_t>(SetInventoryMessage.slot());
    const TInventorySlot item = SetInventoryMessage.has_item()
        ? TInventorySlot::fromProtocolInventoryItem(SetInventoryMessage.item())
        : TInventorySlot{};
    tibia::game::processInventoryChange(slot, item);
}

void TContainerProtocolMessageHandler::handleDeleteInventoryMessage(
    const GameserverMessageDeleteInventory& DeleteInventoryMessage)
{
    // Same side effects as Game::processInventoryChange(slot, {}) / parseRemoveInventoryItem.
    tibia::game::processInventoryChange(static_cast<uint8_t>(DeleteInventoryMessage.slot()), {});
}

void TContainerProtocolMessageHandler::handleSpecialContainersAvailableMessage(
    const GameserverMessageSpecialContainersAvailable& SpecialContainersAvailableMessage)
{
    // Same side effects as ProtocolGame::parseSpecialContainer.
    const bool stash = SpecialContainersAvailableMessage.stash() != 0;
    const bool market = SpecialContainersAvailableMessage.market() != 0;
    if (const auto& player = tibia::game::localPlayer()) {
        player->setSupplyStashAvailable(stash);
        player->setMarketAvailable(market);
    }
    tibia::game::notify("onSpecialContainersAvailable", stash, market);
}

void TContainerProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                               bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    switch (opcode) {
    case 42: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(GameserverMessageSpecialContainersAvailable::
                                         gameserver_message_special_containers_available))
            return;
        handleSpecialContainersAvailableMessage(shadow->GetExtension(
            GameserverMessageSpecialContainersAvailable::
                gameserver_message_special_containers_available));
        break;
    }
    case 111: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(GameserverMessageCloseContainer::gameserver_message_close_container))
            return;
        handleCloseContainerMessage(
            shadow->GetExtension(GameserverMessageCloseContainer::gameserver_message_close_container));
        break;
    }
    case 120: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(GameserverMessageSetInventory::gameserver_message_set_inventory))
            return;
        handleSetInventoryMessage(
            shadow->GetExtension(GameserverMessageSetInventory::gameserver_message_set_inventory));
        break;
    }
    case 121: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(
                GameserverMessageDeleteInventory::gameserver_message_delete_inventory))
            return;
        handleDeleteInventoryMessage(shadow->GetExtension(
            GameserverMessageDeleteInventory::gameserver_message_delete_inventory));
        break;
    }
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    auto* handler = new TContainerProtocolMessageHandler(parent);
    TContainerProtocolMessageHandler::setLiveInstance(handler);
}

uint32_t TInventorySlot::durationRemainingSeconds() const
{
    if (decaying) {
        const auto remaining = durationEndMs - g_clock.millis();
        return remaining > 0 ? static_cast<uint32_t>(remaining / 1000) : 0;
    }
    return durationSeconds;
}

TInventorySlot TInventorySlot::fromProtocolObject(ThingType* type, const uint16_t countOrSubType, const uint8_t tier,
                                                  const uint8_t hangSide)
{
    TInventorySlot slot;
    if (!type || type->getId() == 0)
        return slot;

    slot.typeId = type->getId();
    slot.stackable = type->isStackable();
    slot.fluidOrSplash = type->isFluidContainer() || type->isSplash();
    slot.count = slot.stackable ? countOrSubType : 0;
    slot.fluid = slot.fluidOrSplash ? static_cast<uint8_t>(std::clamp<uint16_t>(countOrSubType, 0, 255)) : 0;
    slot.tier = tier;
    slot.hasWearOut = type->hasWearOut();
    slot.hasClock = type->hasClockExpire() || type->hasExpire() || type->hasExpireStop();
    slot.dualWield = type->isDualWield();

    const uint16_t count = slot.stackable ? slot.count : 0;
    const uint8_t subType = slot.fluidOrSplash ? slot.fluid : 0;
    slot.appearance = appearances::TAppearanceInstanceFactory::fromProtocolObject(type, count, subType, tier,
                                                                                    slot.fluid, hangSide);
    return slot;
}

TInventorySlot TInventorySlot::fromTypeId(const uint16_t typeId, const uint16_t countOrSubType, const uint8_t tier)
{
    return fromProtocolObject(tibia::game::rawThingType(typeId, ThingCategoryItem), countOrSubType, tier);
}

TInventorySlot TInventorySlot::fromProtocolInventoryItem(const ProtocolInventoryItem& item)
{
    const uint16_t typeId = static_cast<uint16_t>(item.item_id());
    ThingType* type = tibia::game::rawThingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return {};

    const uint16_t countOrSubType =
        static_cast<uint16_t>(item.has_count_or_subtype() ? item.count_or_subtype() : 0);
    const uint8_t tier = static_cast<uint8_t>(item.has_tier() ? item.tier() : 0);

    TInventorySlot slot = fromProtocolObject(type, countOrSubType, tier);

    if (item.has_loot_flags())
        slot.lootFlags = item.loot_flags();
    if (item.has_obtain_flags())
        slot.obtainFlags = item.obtain_flags();
    if (item.has_has_container_ammo() && item.has_container_ammo())
        slot.containerAmmoTotal = item.has_container_ammo_total() ? item.container_ammo_total() : 0;

    if (item.has_duration_seconds()) {
        slot.durationSeconds = item.duration_seconds();
        slot.hasClock = true;
    }
    if (item.has_charges()) {
        slot.charges = item.charges();
        slot.hasWearOut = true;
    }
    if (item.has_brand_new())
        slot.brandNew = item.brand_new();
    if (item.has_deco_item_id())
        slot.decoItemId = static_cast<uint16_t>(item.deco_item_id());

    if (item.has_has_podium() && item.has_podium()) {
        Outfit podiumOutfit;
        const uint16_t looktype = static_cast<uint16_t>(item.has_look_type() ? item.look_type() : 0);
        if (looktype != 0) {
            podiumOutfit.setCategory(ThingCategoryCreature);
            uint16_t lookId = looktype;
            if (!tibia::game::isValidDatId(lookId, ThingCategoryCreature)) {
                g_logger.traceError("invalid podium looktype {}", lookId);
                lookId = 0;
            }
            podiumOutfit.setId(lookId);
            podiumOutfit.setHead(uint8_t(item.has_look_head() ? item.look_head() : 0));
            podiumOutfit.setBody(uint8_t(item.has_look_body() ? item.look_body() : 0));
            podiumOutfit.setLegs(uint8_t(item.has_look_legs() ? item.look_legs() : 0));
            podiumOutfit.setFeet(uint8_t(item.has_look_feet() ? item.look_feet() : 0));
            podiumOutfit.setAddons(uint8_t(item.has_look_addons() ? item.look_addons() : 0));
        } else {
            uint16_t lookTypeEx = static_cast<uint16_t>(item.has_look_type_ex() ? item.look_type_ex() : 0);
            if (lookTypeEx != 0) {
                if (!tibia::game::isValidDatId(lookTypeEx, ThingCategoryItem)) {
                    g_logger.traceError("invalid podium looktypeex {}", lookTypeEx);
                    lookTypeEx = 0;
                }
                if (lookTypeEx != 0) {
                    podiumOutfit.setCategory(ThingCategoryItem);
                    podiumOutfit.setAuxId(lookTypeEx);
                }
            }
        }

        const uint16_t lookmount = static_cast<uint16_t>(item.has_look_mount() ? item.look_mount() : 0);
        if (lookmount != 0) {
            podiumOutfit.setMount(lookmount);
            podiumOutfit.setMountColors(uint8_t(item.has_look_mount_head() ? item.look_mount_head() : 0),
                                        uint8_t(item.has_look_mount_body() ? item.look_mount_body() : 0),
                                        uint8_t(item.has_look_mount_legs() ? item.look_mount_legs() : 0),
                                        uint8_t(item.has_look_mount_feet() ? item.look_mount_feet() : 0));
        }

        const uint8_t podiumDirection =
            uint8_t(item.has_podium_direction() ? item.podium_direction() : 0);
        const bool podiumVisible = item.has_podium_visible() && item.podium_visible();
        if (const auto showOff =
                std::dynamic_pointer_cast<appearances::TShowOffSocketAppearanceInstance>(slot.appearance))
            showOff->setLook(podiumOutfit, podiumDirection, podiumVisible);
    }

    return slot;
}

TInventorySlot TInventorySlot::fromItem(const ItemPtr& item)
{
    TInventorySlot slot;
    if (!item)
        return slot;

    slot.appearance = appearances::TAppearanceInstanceFactory::fromItem(*item);
    slot.typeId = static_cast<uint16_t>(item->getId());
    slot.stackable = item->isStackable();
    slot.fluidOrSplash = item->isFluidContainer() || item->isSplash();
    slot.count = slot.stackable ? static_cast<uint16_t>(std::clamp(item->getCountOrSubType(), 0, 65535)) : 0;
    slot.fluid = slot.fluidOrSplash ? static_cast<uint8_t>(std::clamp(item->getCountOrSubType(), 0, 255)) : 0;
    slot.tier = item->getTier();
    slot.decoItemId = item->getDecoItemId();
    slot.charges = item->getCharges();
    slot.decaying = item->isDecaying();
    slot.durationSeconds = item->getDurationTime();
    if (slot.decaying)
        slot.durationEndMs = g_clock.millis() + static_cast<int64_t>(slot.durationSeconds) * 1000;
    slot.containerAmmoTotal = item->getContainerAmmoTotal();
    slot.lootFlags = item->getLootFlags();
    slot.obtainFlags = item->getObtainFlags();
    slot.brandNew = item->isBrandNew();
    slot.hasWearOut = item->hasWearOut();
    slot.hasClock = item->hasClockExpire() || item->hasExpire() || item->hasExpireStop();
    slot.dualWield = item->isDualWield();
    return slot;
}

TContainerStorage::TContainerStorage(QObject* parent)
    : cip::tibia::container::TContainerStorage(parent)
{
}

TContainerStorage::~TContainerStorage()
{
    if (g_containerStorage == this)
        g_containerStorage = nullptr;
}

TContainerStorage* TContainerStorage::instance()
{
    return g_containerStorage;
}

void TContainerStorage::setInstance(TContainerStorage* storage)
{
    g_containerStorage = storage;
}

TStoredContainer* TContainerStorage::mutableAt(const TContainerID id)
{
    const auto it = m_containers.find(id);
    return it == m_containers.end() ? nullptr : &it.value();
}

void TContainerStorage::emitUpdated(const TContainerID id, const bool isNew)
{
    Q_EMIT containerUpdated(id, isNew);
}

void TContainerStorage::applyOpen(const TContainerID id, const uint8_t capacity, QString name, TInventorySlot containerItem,
                                  const bool hasParent, const bool unlocked, const bool hasPages, const uint16_t size,
                                  const uint16_t firstIndex, const std::vector<TInventorySlot>& items)
{
    TStoredContainer row;
    row.id = id;
    row.capacity = capacity;
    row.name = std::move(name);
    row.containerItem = std::move(containerItem);
    row.hasParent = hasParent;
    row.unlocked = unlocked;
    row.hasPages = hasPages;
    row.size = size;
    row.firstIndex = firstIndex;
    row.items = items;
    m_containers.insert(id, std::move(row));
    // Open and replace-open both pass isNew=true so UI creates or resets scroll in place.
    emitUpdated(id, true);
}

void TContainerStorage::closeContainer(const TContainerID id)
{
    if (!m_containers.remove(id))
        return;
    Q_EMIT containerRemoved(id);
}

void TContainerStorage::applyAddItem(const TContainerID id, const TInventorySlot& item, const uint16_t slot)
{
    TStoredContainer* row = mutableAt(id);
    if (!row)
        return;

    int pageSlot = static_cast<int>(slot) - static_cast<int>(row->firstIndex);

    // Item landed on a later page: size only.
    if (row->hasPages && pageSlot > static_cast<int>(row->capacity)) {
        ++row->size;
        emitUpdated(id, false);
        return;
    }

    if (static_cast<int>(row->items.size()) == static_cast<int>(row->capacity)) {
        applyRemoveItem(id, static_cast<uint16_t>(row->firstIndex + row->capacity - 1), {});
        row = mutableAt(id);
        if (!row)
            return;
        ++row->size;
        pageSlot = static_cast<int>(slot) - static_cast<int>(row->firstIndex);
    }

    if (pageSlot < 0 || pageSlot > static_cast<int>(row->items.size())) {
        g_logger.traceError("container add slot out of range");
        return;
    }

    row->items.insert(row->items.begin() + pageSlot, item);
    ++row->size;
    emitUpdated(id, false);
}

void TContainerStorage::applyUpdateItem(const TContainerID id, const uint16_t slot, const TInventorySlot& item)
{
    TStoredContainer* row = mutableAt(id);
    if (!row)
        return;
    const int pageSlot = static_cast<int>(slot) - static_cast<int>(row->firstIndex);
    if (pageSlot < 0 || pageSlot >= static_cast<int>(row->items.size())) {
        g_logger.traceError("container update slot not found");
        return;
    }
    row->items[static_cast<size_t>(pageSlot)] = item;
    emitUpdated(id, false);
}

void TContainerStorage::applyRemoveItem(const TContainerID id, const uint16_t slot, const TInventorySlot& lastItem)
{
    TStoredContainer* row = mutableAt(id);
    if (!row)
        return;

    int pageSlot = static_cast<int>(slot) - static_cast<int>(row->firstIndex);

    // Deleted from a later page: size only.
    if (row->hasPages && pageSlot >= static_cast<int>(row->items.size())) {
        if (row->size > 0)
            --row->size;
        emitUpdated(id, false);
        return;
    }

    if (pageSlot < 0 || pageSlot >= static_cast<int>(row->items.size())) {
        g_logger.traceError("container remove slot not found");
        return;
    }

    row->items.erase(row->items.begin() + pageSlot);

    if (!lastItem.empty()) {
        applyAddItem(id, lastItem, static_cast<uint16_t>(row->firstIndex + row->capacity - 1));
        row = mutableAt(id);
        if (!row)
            return;
        if (row->size > 0)
            --row->size;
    }

    if (row->size > 0)
        --row->size;
    emitUpdated(id, false);
}

const TStoredContainer* TContainerStorage::containerAt(const TContainerID id) const
{
    const auto it = m_containers.constFind(id);
    return it == m_containers.cend() ? nullptr : &it.value();
}

TInventorySlot TContainerStorage::slotAt(const TContainerID id, const int slotId) const
{
    const TStoredContainer* row = containerAt(id);
    if (!row)
        return {};
    const int index = slotId - static_cast<int>(row->firstIndex);
    if (index < 0 || index >= static_cast<int>(row->items.size()))
        return {};
    return row->items[static_cast<size_t>(index)];
}

QList<TContainerID> TContainerStorage::openIds() const
{
    return m_containers.keys();
}

bool TContainerStorage::hasContainer(const TContainerID id) const
{
    return m_containers.contains(id);
}

int TContainerStorage::findEmptyId() const
{
    int id = 0;
    while (m_containers.contains(id))
        ++id;
    return id;
}

void TContainerStorage::clear()
{
    if (m_containers.isEmpty())
        return;
    const QList<TContainerID> ids = m_containers.keys();
    m_containers.clear();
    for (const TContainerID id : ids)
        Q_EMIT containerRemoved(id);
}

TInventoryContainer::TInventoryContainer(QObject* parent)
    : cip::tibia::container::TInventoryContainer(parent)
{
}

TInventoryContainer::~TInventoryContainer()
{
    if (g_inventoryContainer == this)
        g_inventoryContainer = nullptr;
}

TInventoryContainer* TInventoryContainer::instance()
{
    return g_inventoryContainer;
}

void TInventoryContainer::setInstance(TInventoryContainer* storage)
{
    g_inventoryContainer = storage;
}

void TInventoryContainer::setSlot(const int slotId, const TInventorySlot& slot)
{
    if (slotId < 0 || slotId >= SlotCount)
        return;
    m_slots[static_cast<size_t>(slotId)] = slot;
    Q_EMIT inventoryChanged();
    Q_EMIT inventorySlotChanged(slotId);
}

void TInventoryContainer::clearSlot(const int slotId)
{
    setSlot(slotId, {});
}

void TInventoryContainer::clear()
{
    for (int i = 0; i < SlotCount; ++i) {
        if (!m_slots[static_cast<size_t>(i)].empty())
            clearSlot(i);
    }
}

const TInventorySlot& TInventoryContainer::slotAt(const int slotId) const
{
    if (slotId < 0 || slotId >= SlotCount)
        return m_empty;
    return m_slots[static_cast<size_t>(slotId)];
}

bool TInventoryContainer::hasSlot(const int slotId) const
{
    return slotId >= 0 && slotId < SlotCount && !m_slots[static_cast<size_t>(slotId)].empty();
}

} // namespace tibia::container
