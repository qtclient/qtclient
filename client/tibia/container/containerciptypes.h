#pragma once

#include <map>
#include <utility>

// CIP-named container surfaces (promoted from cipwire).

#include "appearances/appearanceinstance.h"
#include "appearances/objectappearanceinstancetypeid.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/const.h>
#include <client/declarations.h>
#include <protocol.pb.h>

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tibia::appearances {
class TAppearanceType;
}
using TAppearanceType = tibia::appearances::TAppearanceType;

// Classic nested braces (qtclient + pack TUs share /FI framework/pch.h).
namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageChangeInContainer;
class GameserverMessageCloseContainer;
class GameserverMessageContainer;
class GameserverMessageCreateInContainer;
class GameserverMessageDeleteInContainer;
class GameserverMessageDeleteInventory;
class GameserverMessageSetInventory;
class GameserverMessageShowMessageDialog;
class GameserverMessageSpecialContainersAvailable;
class GameserverMessagePlayerInventory;
class GameserverMessageObjectInfo;
class GameserverMessageStash;
class GameserverMessageDepotSearchResult;
class GameserverMessageDepotSearchDetailList;
class GameserverMessageCloseDepotSearch;
class ProtocolInventoryItem;
} // namespace protocol
} // namespace protobuf

namespace container {

using TContainerID = int;

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// One inventory / container slot's appearance payload (CIP object-info).
/// Controllers read this.
struct TInventorySlot
{
    appearances::TAppearanceInstancePtr appearance;
    uint16_t typeId = 0;
    uint16_t count = 0;
    uint8_t fluid = 0;
    uint8_t tier = 0;
    uint16_t decoItemId = 0;
    uint32_t charges = 0;
    uint32_t durationSeconds = 0;
    int64_t durationEndMs = 0;
    bool decaying = false;
    /// A quiver: the slot shows containerAmmoTotal, zero included.
    bool hasContainerAmmo = false;
    uint32_t containerAmmoTotal = 0;
    uint32_t lootFlags = 0;
    uint32_t obtainFlags = 0;
    bool brandNew = false;
    bool hasWearOut = false;
    bool hasClock = false;
    bool stackable = false;
    bool fluidOrSplash = false;
    bool dualWield = false;

    bool empty() const { return typeId == 0; }
    uint32_t durationRemainingSeconds() const;
    /// Time left in milliseconds (a decaying item counts down from its end time).
    int64_t durationRemainingMilliseconds() const;

    /// Bridge from protocol rows.
    /// Build from TAppearanceType + protocol object fields (no Item::create).
    static TInventorySlot fromProtocolObject(TAppearanceType* type, uint16_t countOrSubType = 0, uint8_t tier = 0,
                                             uint8_t hangSide = 0);
    /// TypeId-only (NPC trade list rows, classification helpers).
    static TInventorySlot fromTypeId(uint16_t typeId, uint16_t countOrSubType = 0, uint8_t tier = 0);
    /// Rebuild from structured ProtocolInventoryItem (classic getInventorySlot schema).
    static TInventorySlot fromProtocolInventoryItem(
        const ::tibia::protobuf::protocol::ProtocolInventoryItem& item);
};

/// An open container (CIP: TContainer): its metadata and the slots of the page
/// shown. A slot number counts over all pages; the page holds `capacity` slots.
/// The protocol handler builds a container through the setters.
class TContainer
{
public:
    TContainer& setId(uint8_t containerId)
    {
        m_id = containerId;
        return *this;
    }
    /// Also sets the capitalised name and whether this is the browse field.
    TContainer& setName(const QString& containerName);
    TContainer& setCapacity(uint32_t slots)
    {
        m_capacity = static_cast<uint8_t>(slots);
        return *this;
    }
    /// A size of 0xffff or more means there may be more pages.
    void setSize(uint32_t objects);
    TContainer& setFirstIndex(uint32_t index)
    {
        m_firstIndex = static_cast<uint16_t>(index);
        return *this;
    }
    /// The server's category; the category shown becomes it as well.
    TContainer& setServerFilterCategory(uint32_t category)
    {
        m_serverFilterCategory = category;
        m_filterCategory = category;
        return *this;
    }
    TContainer& setHasPages(bool value)
    {
        m_hasPages = value;
        return *this;
    }
    TContainer& setUnlocked(bool value)
    {
        m_unlocked = value;
        return *this;
    }
    TContainer& setHasParent(bool value)
    {
        m_hasParent = value;
        return *this;
    }
    TContainer& setShowSearch(bool value)
    {
        m_showSearch = value;
        return *this;
    }
    TContainer& setContainerItem(const TInventorySlot& item)
    {
        m_containerItem = item;
        return *this;
    }
    TContainer& setSortable(bool value)
    {
        m_sortable = value;
        return *this;
    }
    TContainer& setContentMovableToManagedContainers(bool value)
    {
        m_contentMovableToManagedContainers = value;
        return *this;
    }
    /// Adds a category the server offers; an id already known keeps its name.
    TContainer& addFilterCategory(uint32_t categoryId, const std::string& categoryName)
    {
        m_filterCategories.try_emplace(categoryId, categoryName);
        return *this;
    }
    /// Host concern: qtclient's storage updates the view of an open container in
    /// place, so it drops the categories before adding the server's again.
    void clearFilterCategories() { m_filterCategories.clear(); }

    TContainerID id() const { return m_id; }
    const TInventorySlot& containerItem() const { return m_containerItem; }
    const QString& name() const { return m_name; }
    uint32_t capacity() const { return m_capacity; }
    bool hasParent() const { return m_hasParent; }
    bool showSearch() const { return m_showSearch; }
    bool isUnlocked() const { return m_unlocked; }
    bool hasPages() const { return m_hasPages; }
    uint32_t size() const { return m_size; }
    uint32_t firstIndex() const { return m_firstIndex; }
    /// The objects of the page shown.
    const std::vector<TInventorySlot>& items() const { return m_items; }
    /// The categories the server offers, by id.
    const std::map<uint32_t, std::string>& filterCategories() const { return m_filterCategories; }
    /// The category shown.
    uint32_t filterCategory() const { return m_filterCategory; }
    /// The server sorts this container's content on request.
    bool isSortable() const { return m_sortable; }
    /// The server moves this container's content to the managed containers on request.
    bool isContentMovableToManagedContainers() const { return m_contentMovableToManagedContainers; }

    void setFilterCategory(uint32_t category) { m_filterCategory = category; }
    /// The category shown is not the one the server sent.
    bool isFilterCategoryChanged() const { return m_filterCategory != m_serverFilterCategory; }
    bool isBrowseField() const { return m_browseField; }
    bool hasPossiblyMorePages() const { return m_possiblyMorePages; }
    const QString& displayName() const { return m_capitalizedName; }

    /// Puts tutorial marker `markerId` on slot `index`.
    void setTutorialMarker(int markerId, uint32_t index) { m_tutorialMarkers[index] = markerId; }
    /// The tutorial marker on slot `index`, 0 for none.
    int tutorialMarkerAt(uint32_t index) const
    {
        const auto it = m_tutorialMarkers.find(index);
        return it == m_tutorialMarkers.end() ? 0 : it->second;
    }
    void clearTutorialMarkers() { m_tutorialMarkers.clear(); }
    /// The position of slot `slot` in the page.
    size_t slotIndex(uint32_t slot) const { return m_capacity != 0 ? slot % m_capacity : slot; }
    /// The object in slot `slot`, empty outside the page.
    TInventorySlot itemAt(uint32_t slot) const;
    /// Replaces the object in slot `slot`.
    void setItem(uint32_t slot, const TInventorySlot& item);
    /// Removes the object of slot `slot` when the slot is on the page; one object less in total.
    void removeItem(uint32_t slot);
    /// Inserts an object at slot `slot`; the page keeps `capacity` slots. One object more in total.
    void insertItem(uint32_t slot, const TInventorySlot& item);
    /// Adds an object at the end of the page.
    void appendItem(const TInventorySlot& item) { m_items.push_back(item); }
    size_t itemCount() const { return m_items.size(); }
    void increaseSize() { ++m_size; }
    void decreaseSize() { --m_size; }
    /// Pages of a paged container, 1 otherwise.
    uint32_t pageCount() const;
    /// The page shown, from 1.
    uint32_t currentPage() const;
    bool isLastPage() const { return currentPage() == pageCount(); }
    bool isFirstPage() const { return currentPage() == 1; }
    /// The first slot of the next page (this page's when it is the last).
    uint32_t nextPageFirstIndex() const { return (isLastPage() ? 0 : m_capacity) + m_firstIndex; }
    /// The first slot of the previous page (0 on the first).
    uint32_t previousPageFirstIndex() const { return isFirstPage() ? 0 : m_firstIndex - m_capacity; }

    bool hasFilterCategories() const { return !m_filterCategories.empty(); }
    /// The widget shows its configuration button.
    bool hasConfigurationOptions() const
    {
        return hasFilterCategories() || m_sortable || m_contentMovableToManagedContainers;
    }

private:
    TContainerID m_id = 0;
    TInventorySlot m_containerItem;
    QString m_name;
    /// The name with every word capitalised.
    QString m_capitalizedName;
    uint8_t m_capacity = 0;
    /// The container is the browse field (its name is "Browse Field").
    bool m_browseField = false;
    bool m_hasParent = false;
    /// Show the depot search button on this widget.
    bool m_showSearch = false;
    bool m_unlocked = true;
    bool m_hasPages = false;
    /// The server's size did not fit (0xffff or more): there may be more pages.
    bool m_possiblyMorePages = false;
    uint16_t m_size = 0;
    uint16_t m_firstIndex = 0;
    std::vector<TInventorySlot> m_items;
    /// Tutorial markers by slot index.
    std::map<uint32_t, int> m_tutorialMarkers;
    std::map<uint32_t, std::string> m_filterCategories;
    /// The category the server last sent.
    uint32_t m_serverFilterCategory = 0;
    uint32_t m_filterCategory = 0;
    bool m_sortable = false;
    bool m_contentMovableToManagedContainers = false;
};

using TStoredContainer = TContainer;

/// CIP `container::TContainerProtocolMessageHandler`: the container, inventory,
/// stash and depot search messages, applied to the active session's storages.
/// The stash dialog and the depot search widget follow its request signals.
class TContainerProtocolMessageHandler final : public cip::tibia::container::TContainerProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TContainerProtocolMessageHandler(QObject* parent = nullptr);

    /// CIP slot: CloseContainer (S 0x6F / 111) — native Q→H apply.
    void handleCloseContainerMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCloseContainer& CloseContainerMessage);

    /// CIP slot: Container (S 0x6E / 110) — native Q→H apply.
    void handleContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageContainer& ContainerMessage);

    /// CIP slot: CreateInContainer (S 0x70 / 112) — native Q→H apply.
    void handleCreateInContainerMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCreateInContainer& CreateInContainerMessage);

    /// CIP slot: ChangeInContainer (S 0x71 / 113) — native Q→H apply.
    void handleChangeInContainerMessage(
        const ::tibia::protobuf::protocol::GameserverMessageChangeInContainer& ChangeInContainerMessage);

    /// CIP slot: DeleteInContainer (S 0x72 / 114) — native Q→H apply.
    void handleDeleteInContainerMessage(
        const ::tibia::protobuf::protocol::GameserverMessageDeleteInContainer& DeleteInContainerMessage);

    /// CIP slot: SetInventory (S 0x78 / 120) — native Q→H apply.
    void handleSetInventoryMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSetInventory& SetInventoryMessage);

    /// CIP slot: DeleteInventory (S 0x79 / 121) — native Q→H apply.
    void handleDeleteInventoryMessage(
        const ::tibia::protobuf::protocol::GameserverMessageDeleteInventory& DeleteInventoryMessage);

    /// The stash and the market became available or went away. Without the
    /// stash its content is dropped and the stash dialog closes.
    void handleSpecialContainersAvailableMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable&
            SpecialContainersAvailableMessage);

    void handlePlayerInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerInventory& message);
    /// The names of the instances asked about, into the session's object info storage.
    void handleObjectInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageObjectInfo& ObjectInfoMessage);
    /// The stash's content, into the session's stash storage; the stash dialog
    /// shows or refreshes.
    void handleStashMessage(const ::tibia::protobuf::protocol::GameserverMessageStash& StashMessage);
    /// The message dialogs this handler owns: the stash's message, the
    /// nagging warning and the inventory capacity warning.
    void handleShowMessageDialogMessage(
        const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& ShowMessageDialogMessage);
    void handleDepotSearchResultMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchResult& message);
    void handleDepotSearchDetailListMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchDetailList& message);
    void handleCloseDepotSearchMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseDepotSearch& message);

    void requestCloseContainer(TContainerID ContainerID);
    /// Opens the container's parent in its place.
    void requestUpContainer(TContainerID ContainerID);
    /// Seeks to the first index of the container's next page, keeping its filter category.
    void requestNextPageForContainer(TContainerID ContainerID);
    /// Seeks to the first index of the container's previous page, keeping its filter category.
    void requestPreviousPageForContainer(TContainerID ContainerID);
    /// Seeks to the container's current first index: the server sends the page again.
    void requestUpdateContainer(TContainerID ContainerID);
    /// The names of the instances, answered by the object info message. The data
    /// is the liquid of a liquid instance, the tier of a tiered one, else 0.
    void requestGetObjectsInfo(const std::vector<appearances::TObjectAppearanceInstanceTypeID>& ObjectInstanceTypes);
    void requestGetObjectInfo(const appearances::TObjectAppearanceInstanceTypeID& ObjectInstanceType);
    void requestSortContainer(TContainerID ContainerID, uint32_t SortOption, bool SortBackpacksFirst, bool SortRecursive);
    void requestMoveContentToManagedContainers(TContainerID ContainerID, bool MoveRecursive);

Q_SIGNALS:
    void sendCloseContainerMessage(const ::tibia::protobuf::protocol::GameclientMessageCloseContainer& CloseContainerMessage);
    void sendUpContainerMessage(const ::tibia::protobuf::protocol::GameclientMessageUpContainer& UpContainerMessage);
    void sendSeekInContainerMessage(const ::tibia::protobuf::protocol::GameclientMessageSeekInContainer& SeekInContainerMessage);
    void sendGetObjectInfoMessage(const ::tibia::protobuf::protocol::GameclientMessageGetObjectInfo& GetObjectInfoMessage);
    void sendContainerActionMessage(const ::tibia::protobuf::protocol::GameclientMessageContainerAction& ContainerActionMessage);
};

/// What a container widget shows (CIP: the render provider's container properties).
struct TContainerProperties
{
    QString caption;
    uint32_t numberOfPages = 0;
    uint32_t currentPage = 0;
    uint32_t numberOfContainerSlots = 0;
    uint32_t numberOfFilledContainerSlots = 0;
    bool levelUpButtonVisible = false;
    bool searchButtonVisible = false;
    bool isUpdatable = false;
    bool resetScrollView = false;
    bool hasPossiblyMorePages = false;
    bool configButtonVisible = false;
    std::vector<TInventorySlot> objects;
    uint32_t indexOfFirstVisibleObject = 0;
    TInventorySlot containerItem;
    QList<int> tutorialMarkerIDs;
};

struct TSlotInfo;

class TContainerRenderProvider final : public ::tibia::CipTyped
{
public:
    /// The properties of container `id`; a container that is not open is
    /// captioned container_not_found and has no slots.
    static std::optional<TContainerProperties> containerProperties(TContainerID id, bool resetScrollView);
    /// What every inventory slot shows, by slot. `showExpire` and
    /// `showExpireWhenUnused` are the options showExpireInInventory and
    /// showExpireWhenUnused.
    static std::map<uint32_t, TSlotInfo> inventorySlotInfos(bool showExpire, bool showExpireWhenUnused);
    /// What inventory slot `slotId` shows: its object, the object's text, the
    /// managed container tooltip; a slot without an object is not updatable.
    static TSlotInfo inventorySlotInfo(int slotId, bool showExpire, bool showExpireWhenUnused);
};

/// Session open-containers storage (CIP: TContainerStorage). Installed while TGameClient is live.
class TContainerStorage final : public cip::tibia::container::TContainerStorage
{
    Q_OBJECT
public:
    explicit TContainerStorage(QObject* parent = nullptr);
    ~TContainerStorage() override;

    static TContainerStorage* instance();
    static void setInstance(TContainerStorage* storage);

    /// Full open/replace (protocol open). Emits containerUpdated(id, true) so UI
    /// creates or resets the widget in place (replace-open keeps the controller).
    void applyOpen(TContainerID id, uint8_t capacity, QString name, TInventorySlot containerItem, bool hasParent,
                   bool unlocked, bool hasPages, bool showSearch, uint16_t size, uint16_t firstIndex,
                   const std::vector<TInventorySlot>& items);
    /// The view options of an open container (filter categories, sort and
    /// managed-container support).
    void applyView(TContainerID id, uint32_t filterCategory, std::map<uint32_t, std::string> filterCategories,
                   bool sortable, bool contentMovableToManagedContainers);
    void closeContainer(TContainerID id);
    void applyAddItem(TContainerID id, const TInventorySlot& item, uint16_t slot);
    void applyUpdateItem(TContainerID id, uint16_t slot, const TInventorySlot& item);
    void applyRemoveItem(TContainerID id, uint16_t slot, const TInventorySlot& lastItem);

    const TStoredContainer* containerAt(TContainerID id) const;
    TInventorySlot slotAt(TContainerID id, int slotId) const;
    QList<TContainerID> openIds() const;
    bool hasContainer(TContainerID id) const;
    /// The lowest free container window id, at most MaxContainerId.
    int findEmptyId() const;
    /// The page navigation of container `id` (0 when it is not open).
    uint32_t nextPageFirstIndex(TContainerID id) const;
    uint32_t previousPageFirstIndex(TContainerID id) const;
    uint32_t slotIndex(TContainerID id, uint32_t slot) const;
    /// Tutorial marker `markerId` on inventory slot `slotId`.
    void setInventoryTutorialMarker(int markerId, int slotId);
    /// Tutorial marker `markerId` on slot `slot` (0xff: the first) of
    /// container `id`; containerUpdated(id, false).
    void setContainerTutorialMarker(int markerId, TContainerID id, int slot);
    /// Clears the inventory's and every container's markers; containerUpdated
    /// for each container.
    void clearAllTutorialMarkers();
    void clear();

    /// The category the container shows, 0 for all.
    uint32_t filterCategory(TContainerID id) const;
    void setFilterCategory(TContainerID id, uint32_t category);
    /// The stash and the market, as the server last announced them.
    bool isStashAvailable() const { return m_stashAvailable; }
    void setStashAvailable(bool available) { m_stashAvailable = available; }
    bool isMarketAvailable() const { return m_marketAvailable; }
    void setMarketAvailable(bool available) { m_marketAvailable = available; }
    /// Manual sort mode, one for all containers of the session.
    static bool manualSortMode();
    void setManualSortMode(bool enabled);

Q_SIGNALS:
    void manualSortModeChanged();
    void containerRemoved(tibia::container::TContainerID containerId);
    void containerUpdated(tibia::container::TContainerID containerId, bool isNewContainer);

private:
    TStoredContainer* mutableAt(TContainerID id);
    void emitUpdated(TContainerID id, bool isNew);

    /// The highest container window id.
    static constexpr int MaxContainerId = 31;
    /// Ordered by id, as CIP keeps them.
    QMap<TContainerID, TStoredContainer> m_containers;
    bool m_stashAvailable = false;
    bool m_marketAvailable = false;
};


/// Session inventory equipment slots (CIP: TInventoryContainer).
class TInventoryContainer final : public cip::tibia::container::TInventoryContainer
{
    Q_OBJECT
public:
    static constexpr int SlotCount = tibia::client::LastInventorySlot;

    explicit TInventoryContainer(QObject* parent = nullptr);
    ~TInventoryContainer() override;

    static TInventoryContainer* instance();
    static void setInstance(TInventoryContainer* storage);

    void setSlot(int slotId, const TInventorySlot& slot);
    void clearSlot(int slotId);
    void clear();

    const TInventorySlot& slotAt(int slotId) const;
    bool hasSlot(int slotId) const;
    /// An equipment slot (head .. ammo) holds an object of `typeId`; for a
    /// classified type only with tier `tier`, and never without one.
    bool isObjectEquipped(uint16_t typeId, std::optional<uint8_t> tier) const;

    /// Puts tutorial marker `markerId` on slot `slotId`; inventoryChanged.
    void setTutorialMarker(int markerId, int slotId);
    /// The tutorial marker on slot `slotId`, 0 for none.
    int tutorialMarker(int slotId) const;
    /// inventoryChanged.
    void clearTutorialMarkers();

Q_SIGNALS:
    // Re-emit CIP inventoryChanged with slot id for UI refresh (base has void inventoryChanged()).
    void inventorySlotChanged(int slotId);

private:
    std::array<TInventorySlot, SlotCount> m_slots{};
    TInventorySlot m_empty;
    std::map<int, int> m_tutorialMarkers;
};

/// Object counts keyed by type and tier: the player's inventory counts (the
/// session owns one), the forge's and the stash's.
class TObjectCountStorage : public cip::tibia::container::TObjectCountStorage
{
    Q_OBJECT
public:
    using TCounts = std::map<std::pair<uint16_t, uint8_t>, uint32_t>;
    using cip::tibia::container::TObjectCountStorage::TObjectCountStorage;

    /// Replaces the counts; objectCountsChanged only when they differ.
    virtual void setCounts(const TCounts& counts);
    /// Drops every count; objectCountsChanged only when there were some.
    virtual void clear();
    virtual const TCounts& counts() const { return m_counts; }
    /// The count of the untiered type.
    virtual uint32_t count(uint16_t typeId) const { return count(typeId, 0); }
    /// 0 when the type and tier have no count.
    virtual uint32_t count(uint16_t typeId, uint8_t tier) const;

private:
    TCounts m_counts;
};

/// The supply stash's content: counts by type, tier 0 (CIP: TStashStorage,
/// an object count storage).
class TStashStorage final : public TObjectCountStorage
{
    Q_OBJECT
public:
    explicit TStashStorage(QObject* parent = nullptr) : TObjectCountStorage(parent) {}
};

/// The depot search's detail list of one object type (CIP:
/// TDepotSearchDetailListStorage): the type and tier, the depot's and the
/// inbox's objects of it with their totals, and the stash's counts.
class TDepotSearchDetailListStorage final : public cip::tibia::container::TDepotSearchDetailListStorage
{
    Q_OBJECT
public:
    explicit TDepotSearchDetailListStorage(QObject* parent = nullptr);

    /// No type, no objects, no stash counts; detailListChanged.
    void clear();
    /// The detail list the server sent; detailListChanged.
    void setDetailList(uint16_t typeId, std::optional<uint8_t> tier, std::vector<TInventorySlot> depotObjects, uint32_t depotCount,
                       std::vector<TInventorySlot> inboxObjects, uint32_t inboxCount, const TObjectCountStorage::TCounts& stashCounts);

    /// 0 while no detail list is shown.
    uint16_t typeId() const { return m_typeId; }
    std::optional<uint8_t> tier() const { return m_tier; }
    const std::vector<TInventorySlot>& depotObjects() const { return m_depotObjects; }
    uint32_t depotCount() const { return m_depotCount; }
    const std::vector<TInventorySlot>& inboxObjects() const { return m_inboxObjects; }
    uint32_t inboxCount() const { return m_inboxCount; }
    const TObjectCountStorage::TCounts& stashCounts() const { return m_stashCounts->counts(); }
    const std::shared_ptr<TObjectCountStorage>& stashCountStorage() const { return m_stashCounts; }
    /// The number of stash entries.
    uint32_t stashCount() const { return static_cast<uint32_t>(m_stashCounts->counts().size()); }

private:
    uint16_t m_typeId = 0;
    std::optional<uint8_t> m_tier;
    std::vector<TInventorySlot> m_depotObjects;
    uint32_t m_depotCount = 0;
    std::vector<TInventorySlot> m_inboxObjects;
    uint32_t m_inboxCount = 0;
    std::shared_ptr<TObjectCountStorage> m_stashCounts;
};

} // namespace container
} // namespace tibia
