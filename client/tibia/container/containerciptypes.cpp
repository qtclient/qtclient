#include "game/gamesessionbase.h"
#include "container/containerciptypes.h"
#include "game/gamesessionstate.h"

#include "appearances/appearanceinstance.h"
#include "protocol/protocolciptypes.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "tibia/client/outfit.h"
#include "tibia/game/gamenotify.h"
#include "tibia/game/gamesession.h"
#include <tibia/game/gamesession.h>

#include "clockservice.h"
#include <QtGlobal>
#include <format>

#include <protocol.pb.h>

#include <QObject>

#include <algorithm>
#include <tibia/game/thingsession_forward.h>
#include <client/staticdata.h>
#include "cyclopedia/iteminfostorage.h"

#include <limits>
#include <map>

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

using ::tibia::protobuf::protocol::GameserverMessageChangeInContainer;
using ::tibia::protobuf::protocol::GameserverMessageCloseContainer;
using ::tibia::protobuf::protocol::GameserverMessageContainer;
using ::tibia::protobuf::protocol::GameserverMessageCreateInContainer;
using ::tibia::protobuf::protocol::GameserverMessageDeleteInContainer;
using ::tibia::protobuf::protocol::GameserverMessageDeleteInventory;
using ::tibia::protobuf::protocol::GameserverMessageDepotSearchResult;
using ::tibia::protobuf::protocol::GameserverMessagePlayerInventory;
using ::tibia::protobuf::protocol::GameserverMessageCloseDepotSearch;
using ::tibia::protobuf::protocol::GameserverMessageDepotSearchDetailList;
using ::tibia::protobuf::protocol::GameserverMessageObjectInfo;
using ::tibia::protobuf::protocol::GameserverMessageSetInventory;
using ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable;
using ::tibia::protobuf::protocol::GameserverMessageStash;
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
    const auto containerId = static_cast<TContainerID>(CloseContainerMessage.container_id());
    if (TContainerStorage* storage = TContainerStorage::instance())
        storage->closeContainer(containerId);
}

void TContainerProtocolMessageHandler::handleContainerMessage(const GameserverMessageContainer& ContainerMessage)
{
    std::vector<TInventorySlot> items;
    items.reserve(static_cast<size_t>(ContainerMessage.items_size()));
    for (const auto& item : ContainerMessage.items())
        items.push_back(TInventorySlot::fromProtocolInventoryItem(item));
    tibia::game::processOpenContainer(static_cast<uint8_t>(ContainerMessage.container_id()),
                                      TInventorySlot::fromProtocolInventoryItem(ContainerMessage.item()),
                                      ContainerMessage.name(), static_cast<uint8_t>(ContainerMessage.capacity()),
                                      ContainerMessage.has_parent(), items, ContainerMessage.is_unlocked(),
                                      ContainerMessage.has_pages(), ContainerMessage.show_search(),
                                      static_cast<uint16_t>(ContainerMessage.container_size()),
                                      static_cast<uint16_t>(ContainerMessage.first_index()));
    if (TContainerStorage* storage = TContainerStorage::instance()) {
        std::map<uint32_t, std::string> categories;
        for (const auto& category : ContainerMessage.categories())
            categories.emplace(category.id(), category.name());
        storage->applyView(static_cast<TContainerID>(ContainerMessage.container_id()), ContainerMessage.category(),
                           std::move(categories), ContainerMessage.is_moveable(), ContainerMessage.is_holding());
    }
}

void TContainerProtocolMessageHandler::handleCreateInContainerMessage(
    const GameserverMessageCreateInContainer& CreateInContainerMessage)
{
    tibia::game::processContainerAddItem(static_cast<uint8_t>(CreateInContainerMessage.container_id()),
                                         CreateInContainerMessage.has_item()
                                             ? TInventorySlot::fromProtocolInventoryItem(CreateInContainerMessage.item())
                                             : TInventorySlot{},
                                         static_cast<uint16_t>(CreateInContainerMessage.slot()));
}

void TContainerProtocolMessageHandler::handleChangeInContainerMessage(
    const GameserverMessageChangeInContainer& ChangeInContainerMessage)
{
    tibia::game::processContainerUpdateItem(static_cast<uint8_t>(ChangeInContainerMessage.container_id()),
                                            static_cast<uint16_t>(ChangeInContainerMessage.slot()),
                                            TInventorySlot::fromProtocolInventoryItem(ChangeInContainerMessage.item()));
}

void TContainerProtocolMessageHandler::handleDeleteInContainerMessage(
    const GameserverMessageDeleteInContainer& DeleteInContainerMessage)
{
    // The object that moves up into the last slot of the page, when there is one.
    tibia::game::processContainerRemoveItem(static_cast<uint8_t>(DeleteInContainerMessage.container_id()),
                                            static_cast<uint16_t>(DeleteInContainerMessage.slot()),
                                            DeleteInContainerMessage.has_item()
                                                ? TInventorySlot::fromProtocolInventoryItem(DeleteInContainerMessage.item())
                                                : TInventorySlot{});
}

void TContainerProtocolMessageHandler::handleSetInventoryMessage(
    const GameserverMessageSetInventory& SetInventoryMessage)
{
    const auto slot = static_cast<uint8_t>(SetInventoryMessage.slot());
    const TInventorySlot item = SetInventoryMessage.has_item()
        ? TInventorySlot::fromProtocolInventoryItem(SetInventoryMessage.item())
        : TInventorySlot{};
    tibia::game::processInventoryChange(slot, item);
}

void TContainerProtocolMessageHandler::handleDeleteInventoryMessage(
    const GameserverMessageDeleteInventory& DeleteInventoryMessage)
{
    tibia::game::processInventoryChange(static_cast<uint8_t>(DeleteInventoryMessage.slot()), {});
}

void TContainerProtocolMessageHandler::handleSpecialContainersAvailableMessage(
    const GameserverMessageSpecialContainersAvailable& SpecialContainersAvailableMessage)
{
    const bool stash = SpecialContainersAvailableMessage.stash() != 0;
    const bool market = SpecialContainersAvailableMessage.market() != 0;
    tibia::game::localPlayerSetSupplyStashAvailable(stash);
    tibia::game::localPlayerSetMarketAvailable(market);
    tibia::game::notify("onSpecialContainersAvailable", stash, market);
}

void TContainerProtocolMessageHandler::handleStashMessage(const ::tibia::protobuf::protocol::GameserverMessageStash& message)
{
    std::vector<std::vector<uint32_t>> stash;
    for (const auto& e : message.entries())
        stash.push_back({ e.object_id(), e.amount() });
    tibia::game::notify("onSupplyStashEnter", stash);
}

void TContainerProtocolMessageHandler::handleDepotSearchDetailListMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchDetailList& message)
{
    const auto typeId = static_cast<uint16_t>(message.object().object_id());
    const ThingType* type = tibia::game::rawThingType(typeId, ThingCategoryItem);
    const std::optional<uint8_t> tier =
        type && type->getClassification() > 0 ? std::optional<uint8_t>(static_cast<uint8_t>(message.object().tier())) : std::nullopt;
    std::vector<TInventorySlot> depotObjects;
    for (const auto& item : message.depot_objects())
        depotObjects.push_back(TInventorySlot::fromProtocolInventoryItem(item));
    std::vector<TInventorySlot> inboxObjects;
    for (const auto& item : message.inbox_objects())
        inboxObjects.push_back(TInventorySlot::fromProtocolInventoryItem(item));
    TObjectCountStorage::TCounts stash;
    for (const auto& entry : message.stash())
        stash[{ static_cast<uint16_t>(entry.object_id()), 0 }] += entry.amount();
    if (tibia::game::TGameSessionBase* session = tibia::game::TGameSessionBase::active())
        session->depotSearchDetailListStorage()->setDetailList(typeId, tier, std::move(depotObjects), message.depot_count(),
                                                               std::move(inboxObjects), message.inbox_count(), stash);
    tibia::game::processDepotSearchDetail();
}

void TContainerProtocolMessageHandler::handleCloseDepotSearchMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseDepotSearch&)
{
    tibia::game::processCloseDepotSearch();
}

void TContainerProtocolMessageHandler::handleObjectInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageObjectInfo& message)
{
    // Descriptions are keyed by object id.
    std::vector<std::tuple<uint16_t, std::string>> objects;
    for (const auto& o : message.objects())
        objects.emplace_back(static_cast<uint16_t>(o.object_id()), o.description());
    if (auto* storage = tibia::cyclopedia::TItemInfoStorage::instance())
        storage->applyItemInfo(objects);
    else
        tibia::game::notify("onItemInfo", objects);
}

void TContainerProtocolMessageHandler::handleDepotSearchResultMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchResult& message)
{
    // The locker's counts, by type and tier (CIP: the session's object counts).
    TObjectCountStorage::TCounts counts;
    for (const auto& e : message.entries())
        counts[{ static_cast<uint16_t>(e.object_id()), static_cast<uint8_t>(e.tier()) }] += e.count();
    if (tibia::game::TGameSessionBase* session = tibia::game::TGameSessionBase::active())
        session->objectCountStorage()->setCounts(counts);
    tibia::game::processDepotSearchResults();
}

void TContainerProtocolMessageHandler::handlePlayerInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerInventory& message)
{
    // Counts per (object, tier); the tier only counts for tiered objects.
    std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts;
    for (const auto& e : message.entries()) {
        const auto id = static_cast<uint16_t>(e.object_id());
        const ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
        const uint8_t tier = type && type->getClassification() > 0 ? static_cast<uint8_t>(e.tier()) : 0;
        auto& entry = counts[{ id, tier }];
        entry = static_cast<uint32_t>(std::min<uint64_t>(uint64_t(entry) + e.count(), std::numeric_limits<uint32_t>::max()));
    }
    if (tibia::game::hasLocalPlayer()) {
        if (tibia::game::TGameSessionBase* session = tibia::game::TGameSessionBase::active())
            session->objectCountStorage()->setCounts(counts);
        tibia::game::notify("updateInventoryItems");
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
        const auto remaining = durationEndMs - tibia::shared::TClockService::frameMillis();
        return remaining > 0 ? static_cast<uint32_t>(remaining / 1000) : 0;
    }
    return durationSeconds;
}

int64_t TInventorySlot::durationRemainingMilliseconds() const
{
    if (decaying)
        return std::max<int64_t>(durationEndMs - tibia::shared::TClockService::frameMillis(), 0);
    return int64_t(durationSeconds) * 1000;
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
    if (item.has_has_container_ammo() && item.has_container_ammo()) {
        slot.hasContainerAmmo = true;
        slot.containerAmmoTotal = item.has_container_ammo_total() ? item.container_ammo_total() : 0;
    }

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
                qCritical("%s", std::format("invalid podium looktype {}", lookId).c_str());
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
                    qCritical("%s", std::format("invalid podium looktypeex {}", lookTypeEx).c_str());
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
                                  const bool hasParent, const bool unlocked, const bool hasPages, const bool showSearch,
                                  const uint16_t size, const uint16_t firstIndex, const std::vector<TInventorySlot>& items)
{
    TStoredContainer row;
    row.id = id;
    row.capacity = capacity;
    row.name = std::move(name);
    row.containerItem = std::move(containerItem);
    row.hasParent = hasParent;
    row.unlocked = unlocked;
    row.hasPages = hasPages;
    row.showSearch = showSearch;
    row.size = size;
    row.firstIndex = firstIndex;
    row.items = items;
    m_containers.insert(id, std::move(row));
    // Open and replace-open both pass isNew=true so UI creates or resets scroll in place.
    emitUpdated(id, true);
}

void TContainerStorage::applyView(const TContainerID id, const uint32_t filterCategory,
                                  std::map<uint32_t, std::string> filterCategories, const bool sortable,
                                  const bool contentMovableToManagedContainers)
{
    TStoredContainer* row = mutableAt(id);
    if (!row)
        return;
    row->filterCategory = filterCategory;
    row->filterCategories = std::move(filterCategories);
    row->sortable = sortable;
    row->contentMovableToManagedContainers = contentMovableToManagedContainers;
    emitUpdated(id, false);
}

std::optional<TContainerProperties> TContainerRenderProvider::containerProperties(const TContainerID id,
                                                                                 const bool resetScrollView)
{
    const TContainerStorage* storage = TContainerStorage::instance();
    const TStoredContainer* row = storage ? storage->containerAt(id) : nullptr;
    if (!row)
        return std::nullopt;
    TContainerProperties properties;
    const uint32_t capacity = row->capacity;
    properties.caption = row->name;
    if (row->hasPages && capacity != 0) {
        // Rounded up, at least one page.
        properties.numberOfPages = std::max<uint32_t>(1, (row->size + capacity - 1) / capacity);
        properties.currentPage = row->firstIndex / capacity + 1;
    }
    properties.numberOfContainerSlots = capacity;
    properties.numberOfFilledContainerSlots = static_cast<uint32_t>(row->items.size());
    properties.levelUpButtonVisible = row->hasParent;
    properties.searchButtonVisible = row->showSearch;
    properties.isUpdatable = row->unlocked;
    properties.resetScrollView = resetScrollView;
    properties.hasPossiblyMorePages = row->size >= 0xffff;
    properties.objects = row->items;
    properties.indexOfFirstVisibleObject = row->firstIndex;
    properties.configButtonVisible = row->hasConfigurationOptions();
    properties.containerItem = row->containerItem;
    // The markers of the visible slots, only when one of them has one.
    QList<int> markers;
    markers.reserve(static_cast<qsizetype>(capacity));
    bool anyMarker = false;
    for (uint32_t index = 0; index < capacity; ++index) {
        const int marker = row->tutorialMarkerAt(row->firstIndex + index);
        markers.append(marker);
        anyMarker = anyMarker || marker != 0;
    }
    if (anyMarker)
        properties.tutorialMarkerIDs = std::move(markers);
    return properties;
}

uint32_t TContainerStorage::filterCategory(const TContainerID id) const
{
    const TStoredContainer* row = containerAt(id);
    return row ? row->filterCategory : 0;
}

void TContainerStorage::setFilterCategory(const TContainerID id, const uint32_t category)
{
    if (TStoredContainer* row = mutableAt(id))
        row->filterCategory = category;
}

namespace {
bool g_manualSortMode = false;
}

bool TContainerStorage::manualSortMode()
{
    return g_manualSortMode;
}

void TContainerStorage::setManualSortMode(const bool enabled)
{
    if (g_manualSortMode == enabled)
        return;
    g_manualSortMode = enabled;
    Q_EMIT manualSortModeChanged();
}

void TContainerStorage::requestFilteredContent(const TContainerID id, const uint16_t index, const uint32_t category)
{
    if (!tibia::game::canPerformGameAction())
        return;
    auto message = tibia::protocol::newGameclientMessage(204);
    auto* m = message->MutableExtension(
        ::tibia::protobuf::protocol::GameclientMessageSeekInContainer::gameclient_message_seek_in_container);
    m->set_container_id(static_cast<uint32_t>(id));
    m->set_index(index);
    m->set_primary_type(category);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void TContainerStorage::requestSort(const TContainerID id, const uint32_t sortType, const bool backpacksFirst,
                                    const bool recursive)
{
    if (!tibia::game::canPerformGameAction())
        return;
    auto message = tibia::protocol::newGameclientMessage(117);
    auto* m = message->MutableExtension(
        ::tibia::protobuf::protocol::GameclientMessageContainerAction::gameclient_message_container_action);
    m->set_container_id(static_cast<uint32_t>(id));
    auto* sort = m->mutable_sort();
    sort->set_sort_type(sortType);
    sort->set_first_flag(backpacksFirst);
    sort->set_second_flag(recursive);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void TContainerStorage::requestMoveContentToManagedContainers(const TContainerID id, const bool recursive)
{
    if (!tibia::game::canPerformGameAction())
        return;
    auto message = tibia::protocol::newGameclientMessage(117);
    auto* m = message->MutableExtension(
        ::tibia::protobuf::protocol::GameclientMessageContainerAction::gameclient_message_container_action);
    m->set_container_id(static_cast<uint32_t>(id));
    m->set_move_to_managed_containers(recursive);
    tibia::protocol::sendGameclientMessage(std::move(message));
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
        qCritical("%s", std::format("container add slot out of range").c_str());
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
        qCritical("%s", std::format("container update slot not found").c_str());
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
        qCritical("%s", std::format("container remove slot not found").c_str());
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
    // One container at a time, lowest id first: each removal is signalled
    // while the others are still open.
    while (!m_containers.isEmpty())
        closeContainer(m_containers.firstKey());
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

TDepotSearchDetailListStorage::TDepotSearchDetailListStorage(QObject* parent)
    : cip::tibia::container::TDepotSearchDetailListStorage(parent)
    , m_stashCounts(std::make_shared<TObjectCountStorage>())
{
}

void TDepotSearchDetailListStorage::clear()
{
    m_typeId = 0;
    m_tier.reset();
    m_depotObjects.clear();
    m_depotCount = 0;
    m_inboxObjects.clear();
    m_inboxCount = 0;
    m_stashCounts->clear();
    Q_EMIT detailListChanged();
}

void TDepotSearchDetailListStorage::setDetailList(const uint16_t typeId, const std::optional<uint8_t> tier,
                                                  std::vector<TInventorySlot> depotObjects, const uint32_t depotCount,
                                                  std::vector<TInventorySlot> inboxObjects, const uint32_t inboxCount,
                                                  const TObjectCountStorage::TCounts& stashCounts)
{
    m_typeId = typeId;
    m_tier = tier;
    m_depotObjects = std::move(depotObjects);
    m_depotCount = depotCount;
    m_inboxObjects = std::move(inboxObjects);
    m_inboxCount = inboxCount;
    m_stashCounts->setCounts(stashCounts);
    Q_EMIT detailListChanged();
}

void TObjectCountStorage::setCounts(const TCounts& counts)
{
    if (m_counts == counts)
        return;
    m_counts = counts;
    Q_EMIT objectCountsChanged();
}

void TObjectCountStorage::clear()
{
    if (m_counts.empty())
        return;
    m_counts.clear();
    Q_EMIT objectCountsChanged();
}

uint32_t TObjectCountStorage::count(const uint16_t typeId, const uint8_t tier) const
{
    const auto it = m_counts.find({ typeId, tier });
    return it == m_counts.end() ? 0 : it->second;
}

} // namespace tibia::container
