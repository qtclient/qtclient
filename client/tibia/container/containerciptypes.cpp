#include "game/gamesessionbase.h"
#include "game/currentgamesession.h"
#include "container/containerciptypes.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "input/gameaction.h"

#include <cmath>
#include "game/gamesessionstate.h"

#include "appearances/appearanceinstance.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include <tibia/appearances/appearancetype.h>
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
#include <client/staticdata.h>

#include <limits>
#include <map>

namespace tibia::cyclopedia {
QString capitalizeWords(const QString& name);
}

namespace tibia::container {

namespace {
TContainerStorage* g_containerStorage = nullptr;

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
using ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog;
using ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable;
using ::tibia::protobuf::protocol::GameserverMessageStash;
using ::tibia::protobuf::protocol::ProtocolInventoryItem;
} // namespace

TContainerProtocolMessageHandler::TContainerProtocolMessageHandler(QObject* parent)
    : cip::tibia::container::TContainerProtocolMessageHandler(parent)
{
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
    tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    if (!session)
        return;
    const bool stash = SpecialContainersAvailableMessage.stash() != 0;
    if (!stash) {
        session->stashStorage()->clear();
        Q_EMIT requestCloseStashDialog();
    }
    session->containerStorage()->setStashAvailable(stash);
    session->containerStorage()->setMarketAvailable(SpecialContainersAvailableMessage.market() != 0);
}

void TContainerProtocolMessageHandler::handleStashMessage(const GameserverMessageStash& StashMessage)
{
    tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    if (!session)
        return;
    // A type listed twice keeps its last amount.
    TObjectCountStorage::TCounts counts;
    for (const auto& entry : StashMessage.entries())
        counts[{ static_cast<uint16_t>(entry.object_id()), 0 }] = entry.amount();
    session->stashStorage()->setCounts(counts);
    Q_EMIT requestShowOrRefreshStashDialog();
}

void TContainerProtocolMessageHandler::handleShowMessageDialogMessage(const GameserverMessageShowMessageDialog& ShowMessageDialogMessage)
{
    switch (ShowMessageDialogMessage.dialog_type()) {
    case 0x1e:
        Q_EMIT requestShowStashMessageDialog(QString::fromStdString(ShowMessageDialogMessage.message()));
        break;
    case 0x46:
        Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::SHOW_INVENTORY_CAPACITY_WARNING));
        break;
    case 0x5a:
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowNaggingDialog>(
            QStringLiteral("Warning"), QString::fromStdString(ShowMessageDialogMessage.message()),
            static_cast<qint64>(ShowMessageDialogMessage.extra_byte())));
        break;
    default:
        break;
    }
}

void TContainerProtocolMessageHandler::handleDepotSearchDetailListMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchDetailList& message)
{
    const auto typeId = static_cast<uint16_t>(message.object().object_id());
    const std::optional<uint8_t> tier =
        message.object().has_tier() ? std::optional<uint8_t>(static_cast<uint8_t>(message.object().tier())) : std::nullopt;
    std::vector<TInventorySlot> depotObjects;
    for (const auto& item : message.depot_objects())
        depotObjects.push_back(TInventorySlot::fromProtocolInventoryItem(item));
    std::vector<TInventorySlot> inboxObjects;
    for (const auto& item : message.inbox_objects())
        inboxObjects.push_back(TInventorySlot::fromProtocolInventoryItem(item));
    TObjectCountStorage::TCounts stash;
    // A type listed twice keeps its last amount.
    for (const auto& entry : message.stash())
        stash[{ static_cast<uint16_t>(entry.object_id()), 0 }] = entry.amount();
    if (tibia::game::TGameSessionBase* session = tibia::game::currentGameSession())
        session->depotSearchDetailListStorage()->setDetailList(typeId, tier, std::move(depotObjects), message.depot_count(),
                                                               std::move(inboxObjects), message.inbox_count(), stash);
    Q_EMIT requestOpenDepotSearchWidget();
}

void TContainerProtocolMessageHandler::handleCloseDepotSearchMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseDepotSearch&)
{
    Q_EMIT requestCloseDepotSearchWidget();
}

void TContainerProtocolMessageHandler::handleObjectInfoMessage(const GameserverMessageObjectInfo& ObjectInfoMessage)
{
    using appearances::TObjectAppearanceInstanceTypeID;
    tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    if (!session)
        return;
    for (const auto& object : ObjectInfoMessage.objects()) {
        const QString name = QString::fromStdString(object.description());
        const auto typeId = static_cast<uint16_t>(object.object_id());
        // The data is the liquid of fluid containers and splashes, the tier
        // of classified objects, nothing for the others.
        const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, typeId);
        TObjectAppearanceInstanceTypeID instance = TObjectAppearanceInstanceTypeID::plain(typeId);
        if (type && (type->isSplash() || type->isFluidContainer()))
            instance = TObjectAppearanceInstanceTypeID::withLiquid(typeId, object.data());
        else if (type && type->hasUpgradeClassification())
            instance = TObjectAppearanceInstanceTypeID::withTier(typeId, static_cast<uint8_t>(object.data()));
        session->objectAppearanceInstanceInfoStorage()->setName(instance, name);
    }
}

void TContainerProtocolMessageHandler::handleDepotSearchResultMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchResult& DepotSearchResultMessage)
{
    tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    if (!session)
        return;
    // The locker's counts by type and tier; a type listed twice keeps its last count.
    TObjectCountStorage::TCounts counts;
    for (const auto& entry : DepotSearchResultMessage.entries()) {
        const uint8_t tier = entry.has_tier() ? static_cast<uint8_t>(entry.tier()) : 0;
        counts[{ static_cast<uint16_t>(entry.object_id()), tier }] = entry.count();
    }
    session->objectCountStorage()->setCounts(counts);
    session->depotSearchDetailListStorage()->clear();
    Q_EMIT requestOpenDepotSearchWidget();
}

void TContainerProtocolMessageHandler::requestCloseContainer(const TContainerID ContainerID)
{
    ::tibia::protobuf::protocol::GameclientMessageCloseContainer message;
    message.set_container_id(ContainerID);
    Q_EMIT sendCloseContainerMessage(message);
}

void TContainerProtocolMessageHandler::requestUpContainer(const TContainerID ContainerID)
{
    ::tibia::protobuf::protocol::GameclientMessageUpContainer message;
    message.set_container_id(ContainerID);
    Q_EMIT sendUpContainerMessage(message);
}

namespace {
/// A seek in the container to `index`, with the container's filter category.
::tibia::protobuf::protocol::GameclientMessageSeekInContainer seekMessage(const TContainerID id, const uint32_t index)
{
    ::tibia::protobuf::protocol::GameclientMessageSeekInContainer message;
    message.set_container_id(id);
    message.set_index(index);
    const TContainerStorage* storage = TContainerStorage::instance();
    message.set_primary_type(storage ? storage->filterCategory(id) : 0);
    return message;
}
} // namespace

void TContainerProtocolMessageHandler::requestNextPageForContainer(const TContainerID ContainerID)
{
    const TContainerStorage* storage = TContainerStorage::instance();
    const TStoredContainer* container = storage ? storage->containerAt(ContainerID) : nullptr;
    Q_EMIT sendSeekInContainerMessage(seekMessage(ContainerID, container ? container->nextPageFirstIndex() : 0));
}

void TContainerProtocolMessageHandler::requestPreviousPageForContainer(const TContainerID ContainerID)
{
    const TContainerStorage* storage = TContainerStorage::instance();
    const TStoredContainer* container = storage ? storage->containerAt(ContainerID) : nullptr;
    Q_EMIT sendSeekInContainerMessage(seekMessage(ContainerID, container ? container->previousPageFirstIndex() : 0));
}

void TContainerProtocolMessageHandler::requestUpdateContainer(const TContainerID ContainerID)
{
    const TContainerStorage* storage = TContainerStorage::instance();
    const TStoredContainer* container = storage ? storage->containerAt(ContainerID) : nullptr;
    Q_EMIT sendSeekInContainerMessage(seekMessage(ContainerID, container ? container->firstIndex() : 0));
}

void TContainerProtocolMessageHandler::requestGetObjectsInfo(
    const std::vector<appearances::TObjectAppearanceInstanceTypeID>& ObjectInstanceTypes)
{
    ::tibia::protobuf::protocol::GameclientMessageGetObjectInfo message;
    for (const appearances::TObjectAppearanceInstanceTypeID& instance : ObjectInstanceTypes) {
        auto* object = message.add_objects();
        object->set_object_id(instance.typeId);
        object->set_data(instance.hasLiquid() ? instance.liquid : instance.hasTier() ? instance.tier : 0);
    }
    Q_EMIT sendGetObjectInfoMessage(message);
}

void TContainerProtocolMessageHandler::requestGetObjectInfo(const appearances::TObjectAppearanceInstanceTypeID& ObjectInstanceType)
{
    requestGetObjectsInfo({ ObjectInstanceType });
}

void TContainerProtocolMessageHandler::requestSortContainer(const TContainerID ContainerID, const uint32_t SortOption,
                                                            const bool SortBackpacksFirst, const bool SortRecursive)
{
    ::tibia::protobuf::protocol::GameclientMessageContainerAction message;
    message.set_container_id(ContainerID);
    auto* sort = message.mutable_sort();
    sort->set_sort_type(SortOption);
    sort->set_first_flag(SortBackpacksFirst);
    sort->set_second_flag(SortRecursive);
    Q_EMIT sendContainerActionMessage(message);
}

void TContainerProtocolMessageHandler::requestMoveContentToManagedContainers(const TContainerID ContainerID, const bool MoveRecursive)
{
    ::tibia::protobuf::protocol::GameclientMessageContainerAction message;
    message.set_container_id(ContainerID);
    message.set_move_to_managed_containers(MoveRecursive);
    Q_EMIT sendContainerActionMessage(message);
}

void TContainerProtocolMessageHandler::handlePlayerInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerInventory& message)
{
    using appearances::TObjectAppearanceInstanceTypeID;
    if (tibia::game::TGameSessionBase* session = tibia::game::currentGameSession()) {
        if (TObjectAppearanceInstanceInfoStorage* infos = session->objectAppearanceInstanceInfoStorage().get()) {
            infos->resetCounts();
            for (const auto& e : message.entries()) {
                const auto typeId = static_cast<uint16_t>(e.object_id());
                const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, typeId);
                TObjectAppearanceInstanceTypeID instance = TObjectAppearanceInstanceTypeID::plain(typeId);
                if (type && (type->isSplash() || type->isFluidContainer()))
                    instance = TObjectAppearanceInstanceTypeID::withLiquid(typeId, e.tier());
                else if (type && type->hasUpgradeClassification())
                    instance = TObjectAppearanceInstanceTypeID::withTier(typeId, static_cast<uint8_t>(e.tier()));
                infos->setCount(instance, static_cast<int>(e.count()));
            }
        }
    }
    // Counts per (object, tier); the tier only counts for tiered objects.
    std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts;
    for (const auto& e : message.entries()) {
        const auto id = static_cast<uint16_t>(e.object_id());
        const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id);
        const uint8_t tier = type && type->getClassification() > 0 ? static_cast<uint8_t>(e.tier()) : 0;
        auto& entry = counts[{ id, tier }];
        entry = static_cast<uint32_t>(std::min<uint64_t>(uint64_t(entry) + e.count(), std::numeric_limits<uint32_t>::max()));
    }
    if (tibia::game::hasLocalPlayer()) {
        if (tibia::game::TGameSessionBase* session = tibia::game::currentGameSession())
            session->objectCountStorage()->setCounts(counts);
        tibia::game::notify("updateInventoryItems");
    }
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

TInventorySlot TInventorySlot::fromProtocolObject(TAppearanceType* type, const uint16_t countOrSubType, const uint8_t tier,
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
    slot.hasClock = type->hasExpire() || type->hasExpireStop();
    slot.dualWield = type->isDualWield();

    const uint16_t count = slot.stackable ? slot.count : 0;
    const uint8_t subType = slot.fluidOrSplash ? slot.fluid : 0;
    slot.appearance = appearances::TAppearanceInstanceFactory::installed()->fromProtocolObject(type, count, subType, tier,
                                                                                    slot.fluid, hangSide);
    return slot;
}

TInventorySlot TInventorySlot::fromTypeId(const uint16_t typeId, const uint16_t countOrSubType, const uint8_t tier)
{
    return fromProtocolObject(tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, typeId), countOrSubType, tier);
}

TInventorySlot TInventorySlot::fromProtocolInventoryItem(const ProtocolInventoryItem& item)
{
    const uint16_t typeId = static_cast<uint16_t>(item.item_id());
    TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, typeId);
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
    // A brand-new wearout or expiring object's instance says so (the supply
    // stash takes only those of their kinds).
    if (slot.brandNew) {
        using appearances::TAppearanceInstanceFactory;
        if (std::dynamic_pointer_cast<appearances::TWearoutObjectAppearanceInstance>(slot.appearance))
            slot.appearance = TAppearanceInstanceFactory::installed()->createWearoutObject(type, slot.charges, true);
        else if (std::dynamic_pointer_cast<appearances::TExpireObjectAppearanceInstance>(slot.appearance))
            slot.appearance = TAppearanceInstanceFactory::installed()->createExpireObject(type, 0, true, false);
    }
    if (item.has_deco_item_id())
        slot.decoItemId = static_cast<uint16_t>(item.deco_item_id());

    if (item.has_has_podium() && item.has_podium()) {
        Outfit podiumOutfit;
        const uint16_t looktype = static_cast<uint16_t>(item.has_look_type() ? item.look_type() : 0);
        if (looktype != 0) {
            podiumOutfit.setCategory(ThingCategoryCreature);
            uint16_t lookId = looktype;
            if (!tibia::appearances::TAppearancesManager::instance().isValidDatId(lookId, ThingCategoryCreature)) {
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
                if (!tibia::appearances::TAppearancesManager::instance().isValidDatId(lookTypeEx, ThingCategoryItem)) {
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
        if (std::dynamic_pointer_cast<appearances::TShowOffSocketAppearanceInstance>(slot.appearance)) {
            // The socket carries the outfit and mount instances, made as the
            // factory makes them for a protocol show-off socket.
            using appearances::TAppearanceInstanceFactory;
            std::shared_ptr<appearances::TOutfitAppearanceInstance> figure;
            if (podiumOutfit.isCreature() && podiumOutfit.getId() != 0)
                figure = TAppearanceInstanceFactory::installed()->createOutfit(
                    TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryCreature, podiumOutfit.getId()), podiumOutfit,
                    podiumDirection);
            else if (podiumOutfit.isItem() && podiumOutfit.getAuxId() != 0)
                figure = TAppearanceInstanceFactory::installed()->createObjectOutfit(podiumOutfit.getAuxId());
            std::shared_ptr<appearances::TOutfitAppearanceInstance> mount;
            if (podiumOutfit.hasMount()) {
                Outfit mountLook;
                mountLook.setCategory(ThingCategoryCreature);
                mountLook.setId(podiumOutfit.getMount());
                mountLook.setHead(podiumOutfit.getMountHead());
                mountLook.setBody(podiumOutfit.getMountBody());
                mountLook.setLegs(podiumOutfit.getMountLegs());
                mountLook.setFeet(podiumOutfit.getMountFeet());
                mount = TAppearanceInstanceFactory::installed()->createOutfit(
                    TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryCreature, podiumOutfit.getMount()), mountLook,
                    podiumDirection);
            }
            if (mount && figure)
                figure->setPatterns(podiumDirection, podiumDirection, 1, false);
            slot.appearance = TAppearanceInstanceFactory::installed()->makeTracked<appearances::TShowOffSocketAppearanceInstance>(
                type, podiumVisible, std::move(figure), std::move(mount));
        }
    }

    return slot;
}

TContainerStorage::TContainerStorage(QObject* parent)
    : cip::tibia::container::TContainerStorage(parent)
    , m_inventoryContainer(std::make_shared<TInventoryContainer>())
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
    row.setId(static_cast<uint8_t>(id))
        .setName(name)
        .setCapacity(capacity)
        .setHasPages(hasPages)
        .setUnlocked(unlocked)
        .setHasParent(hasParent)
        .setShowSearch(showSearch)
        .setContainerItem(containerItem)
        .setFirstIndex(firstIndex);
    row.setSize(size);
    for (const TInventorySlot& item : items)
        row.appendItem(item);
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
    row->setServerFilterCategory(filterCategory)
        .setSortable(sortable)
        .setContentMovableToManagedContainers(contentMovableToManagedContainers);
    row->clearFilterCategories();
    for (const auto& [categoryId, categoryName] : filterCategories)
        row->addFilterCategory(categoryId, categoryName);
    emitUpdated(id, false);
}

TContainerRenderProvider::TContainerRenderProvider(std::shared_ptr<TContainerStorage> containerStorage,
                                                   config::TClientOptions* clientOptions)
    : m_containerStorage(std::move(containerStorage))
    , m_clientOptions(clientOptions)
{
}

std::optional<TContainerProperties> TContainerRenderProvider::containerProperties(const TContainerID id,
                                                                                 const bool resetScrollView) const
{
    const TContainerStorage* storage = m_containerStorage.get();
    const TStoredContainer* row = storage ? storage->containerAt(id) : nullptr;
    // A container that is not open shows as "not found", without slots.
    TContainerProperties properties;
    properties.caption = qtTrId("container_not_found");
    if (!row)
        return properties;
    const uint32_t capacity = row->capacity();
    properties.caption = row->name();
    if (row->hasPages() && capacity != 0) {
        // Rounded up, at least one page.
        properties.numberOfPages = std::max<uint32_t>(1, (row->size() + capacity - 1) / capacity);
        properties.currentPage = row->firstIndex() / capacity + 1;
    }
    properties.numberOfContainerSlots = capacity;
    properties.numberOfFilledContainerSlots = static_cast<uint32_t>(row->itemCount());
    properties.levelUpButtonVisible = row->hasParent();
    properties.searchButtonVisible = row->showSearch();
    properties.isUpdatable = row->isUnlocked();
    properties.resetScrollView = resetScrollView;
    properties.hasPossiblyMorePages = row->hasPossiblyMorePages();
    properties.objects = row->items();
    properties.indexOfFirstVisibleObject = row->firstIndex();
    properties.configButtonVisible = row->hasConfigurationOptions();
    properties.containerItem = row->containerItem();
    // The markers of the visible slots, only when one of them has one.
    QList<int> markers;
    markers.reserve(static_cast<qsizetype>(capacity));
    bool anyMarker = false;
    for (uint32_t index = 0; index < capacity; ++index) {
        const int marker = row->tutorialMarkerAt(row->firstIndex() + index);
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
    return row ? row->filterCategory() : 0;
}

void TContainerStorage::setFilterCategory(const TContainerID id, const uint32_t category)
{
    if (TStoredContainer* row = mutableAt(id))
        row->setFilterCategory(category);
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

void TContainerStorage::closeContainer(const TContainerID id)
{
    if (!m_containers.remove(id))
        return;
    Q_EMIT containerRemoved(id);
}

TContainer& TContainer::setName(const QString& containerName)
{
    m_name = containerName;
    m_capitalizedName = tibia::cyclopedia::capitalizeWords(containerName);
    m_browseField = containerName == QStringLiteral("Browse Field");
    return *this;
}

void TContainer::setSize(const uint32_t objects)
{
    m_size = static_cast<uint16_t>(objects);
    if (objects > 0xfffe)
        m_possiblyMorePages = true;
}

TInventorySlot TContainer::itemAt(const uint32_t slot) const
{
    const size_t index = slotIndex(slot);
    return index < m_items.size() ? m_items[index] : TInventorySlot{};
}

void TContainer::setItem(const uint32_t slot, const TInventorySlot& item)
{
    const size_t index = slotIndex(slot);
    if (index < m_items.size())
        m_items[index] = item;
}

void TContainer::removeItem(const uint32_t slot)
{
    if (m_firstIndex <= slot && slot < static_cast<uint32_t>(m_firstIndex) + m_capacity) {
        const size_t index = slotIndex(slot);
        if (index < m_items.size())
            m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
    }
    decreaseSize();
}

void TContainer::insertItem(const uint32_t slot, const TInventorySlot& item)
{
    const size_t index = std::min(slotIndex(slot), m_items.size());
    m_items.insert(m_items.begin() + static_cast<std::ptrdiff_t>(index), item);
    while (m_items.size() > m_capacity)
        m_items.pop_back();
    increaseSize();
}

uint32_t TContainer::pageCount() const
{
    if (!m_hasPages || m_capacity == 0)
        return 1;
    return std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(static_cast<float>(m_size) / static_cast<float>(m_capacity))));
}

uint32_t TContainer::currentPage() const
{
    if (!m_hasPages || m_capacity == 0)
        return 1;
    return m_firstIndex / m_capacity + 1;
}

void TContainerStorage::applyAddItem(const TContainerID id, const TInventorySlot& item, const uint16_t slot)
{
    TContainer* container = mutableAt(id);
    if (!container)
        return;
    // Without the object it landed on another page: only the total grows.
    if (item.empty())
        container->increaseSize();
    else
        container->insertItem(slot, item);
    emitUpdated(id, false);
}

void TContainerStorage::applyUpdateItem(const TContainerID id, const uint16_t slot, const TInventorySlot& item)
{
    TContainer* container = mutableAt(id);
    if (!container)
        return;
    container->setItem(slot, item);
    emitUpdated(id, false);
}

void TContainerStorage::applyRemoveItem(const TContainerID id, const uint16_t slot, const TInventorySlot& lastItem)
{
    TContainer* container = mutableAt(id);
    if (!container)
        return;
    container->removeItem(slot);
    // The object that moves up into the page's last slot.
    if (!lastItem.empty())
        container->appendItem(lastItem);
    emitUpdated(id, false);
}

const TStoredContainer* TContainerStorage::containerAt(const TContainerID id) const
{
    const auto it = m_containers.constFind(id);
    return it == m_containers.cend() ? nullptr : &it.value();
}

TInventorySlot TContainerStorage::slotAt(const TContainerID id, const int slotId) const
{
    const TContainer* container = containerAt(id);
    if (!container || slotId < 0)
        return {};
    return container->itemAt(static_cast<uint32_t>(slotId));
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
    const int maxContainerId = m_maxContainerId;
    while (id <= maxContainerId && m_containers.contains(id))
        ++id;
    return std::min(id, maxContainerId);
}

uint32_t TContainerStorage::nextPageFirstIndex(const TContainerID id) const
{
    const TStoredContainer* row = containerAt(id);
    return row ? row->nextPageFirstIndex() : 0;
}

uint32_t TContainerStorage::previousPageFirstIndex(const TContainerID id) const
{
    const TStoredContainer* row = containerAt(id);
    return row ? row->previousPageFirstIndex() : 0;
}

uint32_t TContainerStorage::slotIndex(const TContainerID id, const uint32_t slot) const
{
    const TStoredContainer* row = containerAt(id);
    return row ? static_cast<uint32_t>(row->slotIndex(slot)) : 0;
}

void TContainerStorage::setInventoryTutorialMarker(const int markerId, const int slotId)
{
    m_inventoryContainer->setTutorialMarker(markerId, slotId);
}

void TContainerStorage::setContainerTutorialMarker(const int markerId, const TContainerID id, const int slot)
{
    TStoredContainer* row = mutableAt(id);
    if (!row)
        return;
    row->setTutorialMarker(markerId, static_cast<uint32_t>(slot != 0xff ? slot : 0));
    emitUpdated(id, false);
}

void TContainerStorage::clearAllTutorialMarkers()
{
    m_inventoryContainer->clearTutorialMarkers();
    for (auto it = m_containers.begin(); it != m_containers.end(); ++it) {
        it.value().clearTutorialMarkers();
        emitUpdated(it.key(), false);
    }
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

TInventoryContainer::~TInventoryContainer() = default;

TInventoryContainer* TInventoryContainer::instance()
{
    const TContainerStorage* storage = TContainerStorage::instance();
    return storage ? storage->inventoryContainer().get() : nullptr;
}

bool TInventoryContainer::isInventorySlot(const int slotId)
{
    return slotId >= tibia::client::InventorySlotHead && slotId <= tibia::client::InventorySlotPurse;
}

uint8_t TInventoryContainer::countObjects(const uint16_t typeId, const uint32_t flags) const
{
    uint8_t total = 0;
    for (int slotId = tibia::client::InventorySlotHead; slotId <= tibia::client::InventorySlotPurse; ++slotId) {
        const TInventorySlot& slot = slotAt(slotId);
        if (slot.empty() || slot.typeId != typeId)
            continue;
        if ((flags & 1) != 0) {
            const auto* upgradeable =
                dynamic_cast<const appearances::TUpgradeableObjectAppearanceInstance*>(slot.appearance.get());
            if (upgradeable && upgradeable->tier() != 0)
                continue;
        }
        total = static_cast<uint8_t>(total + (slot.stackable ? static_cast<uint8_t>(slot.count) : 1));
    }
    return total;
}

void TInventoryContainer::setSlot(const int slotId, const TInventorySlot& slot)
{
    if (!isInventorySlot(slotId))
        return;
    TInventorySlot& stored = m_slots[static_cast<size_t>(slotId)];
    // The same object again changes nothing.
    if (stored.appearance == slot.appearance && stored.typeId == slot.typeId)
        return;
    stored = slot;
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

bool TInventoryContainer::isObjectEquipped(const uint16_t typeId, const std::optional<uint8_t> tier) const
{
    // The equipment slots, head to ammo; the first one holding the type decides.
    for (int slotId = tibia::client::InventorySlotHead; slotId <= tibia::client::InventorySlotAmmo; ++slotId) {
        const TInventorySlot& slot = slotAt(slotId);
        if (slot.empty() || slot.typeId != typeId)
            continue;
        const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, typeId);
        if (!type || !type->hasUpgradeClassification())
            return true;
        return tier.has_value() && slot.tier == *tier;
    }
    return false;
}

void TInventoryContainer::setTutorialMarker(const int markerId, const int slotId)
{
    m_tutorialMarkers[slotId] = markerId;
    Q_EMIT inventoryChanged();
}

int TInventoryContainer::tutorialMarker(const int slotId) const
{
    const auto it = m_tutorialMarkers.find(slotId);
    return it == m_tutorialMarkers.end() ? 0 : it->second;
}

void TInventoryContainer::clearTutorialMarkers()
{
    m_tutorialMarkers.clear();
    Q_EMIT inventoryChanged();
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
