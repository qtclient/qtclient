#pragma once

// CIP-named container surfaces (promoted from cipwire).

#include "appearances/appearanceinstance.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/const.h>
#include <client/declarations.h>

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <vector>

class ThingType;

// Classic nested braces (qtclient + pack TUs share /FI framework/pch.h).
namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageCloseContainer;
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
/// Controllers read this — not ItemPtr. Protocol dual-write fills via fromItem.
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

    /// Bridge from protocol / LocalPlayerSessionThing Item during dual-write.
    static TInventorySlot fromItem(const ItemPtr& item);
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
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

class TContainerRenderProvider final : public ::tibia::CipTyped {};

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

Q_SIGNALS:
    void containerRemoved(tibia::container::TContainerID containerId);
    void containerUpdated(tibia::container::TContainerID containerId, bool isNewContainer);

private:
    TStoredContainer* mutableAt(TContainerID id);
    void emitUpdated(TContainerID id, bool isNew);

    QHash<TContainerID, TStoredContainer> m_containers;
};

class TDepotSearchDetailListStorage final : public cip::tibia::container::TDepotSearchDetailListStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TDepotSearchDetailListStorage::TDepotSearchDetailListStorage;
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

class TObjectCountStorage final : public cip::tibia::container::TObjectCountStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TObjectCountStorage::TObjectCountStorage;
};

class TStashStorage final : public cip::tibia::container::TStashStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TStashStorage::TStashStorage;
};

} // namespace container
} // namespace tibia
