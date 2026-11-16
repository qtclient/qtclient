#pragma once

#include <map>
#include <utility>

// CIP-named container surfaces (promoted from cipwire).

#include "appearances/appearanceinstance.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/const.h>
#include <client/declarations.h>

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

class ThingType;

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
class GameserverMessageSpecialContainersAvailable;
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
    /// Build from ThingType + protocol object fields (no Item::create).
    static TInventorySlot fromProtocolObject(ThingType* type, uint16_t countOrSubType = 0, uint8_t tier = 0,
                                             uint8_t hangSide = 0);
    /// TypeId-only (NPC trade list rows, classification helpers).
    static TInventorySlot fromTypeId(uint16_t typeId, uint16_t countOrSubType = 0, uint8_t tier = 0);
    /// Rebuild from structured ProtocolInventoryItem (classic getInventorySlot schema).
    static TInventorySlot fromProtocolInventoryItem(
        const ::tibia::protobuf::protocol::ProtocolInventoryItem& item);
};

/// Open container row in session storage (CIP TContainer metadata + slots).
struct TStoredContainer
{
    TContainerID id = 0;
    uint8_t capacity = 0;
    QString name;
    TInventorySlot containerItem;
    bool hasParent = false;
    bool unlocked = true;
    bool hasPages = false;
    /// CIP open-container ≥1281: show depot-search button on this widget.
    bool showSearch = false;
    uint16_t size = 0;
    uint16_t firstIndex = 0;
    std::vector<TInventorySlot> items;
    /// The view filter: the category shown, and the categories the server offers by id.
    uint32_t filterCategory = 0;
    std::map<uint32_t, std::string> filterCategories;
    /// The server sorts this container's content on request.
    bool sortable = false;
    /// The server moves this container's content to the managed containers on request.
    bool contentMovableToManagedContainers = false;

    /// Tutorial markers by slot index.
    std::map<uint32_t, int> tutorialMarkers;

    /// The tutorial marker on slot `index`, 0 for none.
    int tutorialMarkerAt(uint32_t index) const
    {
        const auto it = tutorialMarkers.find(index);
        return it == tutorialMarkers.end() ? 0 : it->second;
    }
    bool hasFilterCategories() const { return !filterCategories.empty(); }
    /// The widget shows its configuration button.
    bool hasConfigurationOptions() const
    {
        return hasFilterCategories() || sortable || contentMovableToManagedContainers;
    }
};

class TContainer final : public ::tibia::CipTyped {};

class TContainerGameActionHandler final : public cip::tibia::container::TContainerGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::container::TContainerGameActionHandler::TContainerGameActionHandler;
};

class TContainerGameActionProvider final : public ::tibia::CipTyped {};

/// CIP `container::TContainerProtocolMessageHandler` — native Q→H for CloseContainer /
/// SetInventory / DeleteInventory / SpecialContainersAvailable.
class TContainerProtocolMessageHandler final : public cip::tibia::container::TContainerProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TContainerProtocolMessageHandler(QObject* parent = nullptr);

    static TContainerProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TContainerProtocolMessageHandler* handler);

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

    /// CIP slot: SpecialContainersAvailable (S 0x2A / 42) — native Q→H apply.
    void handleSpecialContainersAvailableMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable&
            SpecialContainersAvailableMessage);

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode);
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

class TContainerRenderProvider final : public ::tibia::CipTyped
{
public:
    /// The properties of open container `id`; nothing when it is not open.
    static std::optional<TContainerProperties> containerProperties(TContainerID id, bool resetScrollView);
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
    /// First free container window id (0..).
    int findEmptyId() const;
    void clear();

    /// The category the container shows, 0 for all.
    uint32_t filterCategory(TContainerID id) const;
    void setFilterCategory(TContainerID id, uint32_t category);
    /// Manual sort mode, one for all containers of the session.
    static bool manualSortMode();
    void setManualSortMode(bool enabled);

    /// The server requests of the configuration menu.
    static void requestFilteredContent(TContainerID id, uint16_t index, uint32_t category);
    static void requestSort(TContainerID id, uint32_t sortType, bool backpacksFirst, bool recursive);
    static void requestMoveContentToManagedContainers(TContainerID id, bool recursive);

Q_SIGNALS:
    void manualSortModeChanged();
    void containerRemoved(tibia::container::TContainerID containerId);
    void containerUpdated(tibia::container::TContainerID containerId, bool isNewContainer);

private:
    TStoredContainer* mutableAt(TContainerID id);
    void emitUpdated(TContainerID id, bool isNew);

    /// Ordered by id, as CIP keeps them.
    QMap<TContainerID, TStoredContainer> m_containers;
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

Q_SIGNALS:
    // Re-emit CIP inventoryChanged with slot id for UI refresh (base has void inventoryChanged()).
    void inventorySlotChanged(int slotId);

private:
    std::array<TInventorySlot, SlotCount> m_slots{};
    TInventorySlot m_empty;
};

class TObjectAppearanceInstanceInfoStorage final : public cip::tibia::container::TObjectAppearanceInstanceInfoStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TObjectAppearanceInstanceInfoStorage::TObjectAppearanceInstanceInfoStorage;
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

class TStashStorage final : public cip::tibia::container::TStashStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TStashStorage::TStashStorage;
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
