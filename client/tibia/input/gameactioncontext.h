#pragma once

#include <client/position.h>

#include <QString>

#include <cstdint>
#include <memory>
#include <utility>

namespace tibia {
namespace trade {
struct TNPCTradeGood;
}
namespace input {

/// CIP's game action context kinds (the v2 of every location and object
/// context): where an object action happens and what it is done to.
enum class EGameActionContextType : int
{
    LOCATION_INVENTORY = 0,
    LOCATION_CONTAINER_WITH_SLOT = 1,
    LOCATION_BATTLE_LIST = 2,
    LOCATION_WORLD_MAP = 3,
    LOCATION_NPC_TRADE = 4,
    LOCATION_PLAYER_TRADE = 5,
    LOCATION_UNKNOWN = 6,
    LOCATION_NPC_CHAT_KEYWORD = 7,
    OBJECT_APPEARANCE_TYPE = 8,
    OBJECT_CREATURE = 9,
    LOCATION_WORLD_MAP_COORDINATE = 10,
    LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION = 11,
    LOCATION_DEPOT_SEARCH_DEPOT_ITEM = 12,
    LOCATION_DEPOT_SEARCH_INBOX_ITEM = 13,
    LOCATION_ACTION_BUTTON = 14,
    LOCATION_TUTORIAL_HINT_DIALOG = 15,
};

/// The base of CIP's location and object action contexts.
class IGameActionContext
{
public:
    virtual ~IGameActionContext() = default;
    /// CIP v2.
    virtual EGameActionContextType gameActionContextType() const = 0;
};
using TGameActionContextPtr = std::shared_ptr<const IGameActionContext>;

template<typename T>
const T* gameActionContextAs(const TGameActionContextPtr& context)
{
    return dynamic_cast<const T*>(context.get());
}

// --- Locations --------------------------------------------------------------

/// CIP input::TLocationInventoryGameActionContext: an inventory slot (+0x8).
class TLocationInventoryGameActionContext final : public IGameActionContext
{
public:
    explicit TLocationInventoryGameActionContext(int slot) : m_slot(slot) {}
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_INVENTORY; }
    int slot() const { return m_slot; }
    bool operator==(const TLocationInventoryGameActionContext& other) const { return m_slot == other.m_slot; }

private:
    int m_slot;
};

/// CIP input::TLocationContainerWithSlotGameActionContext: a slot (+0xc) of an
/// open container window (+0x8). The container provider also notes whether the
/// window shows a loot corpse (+0x10) or the browse field (+0x11).
class TLocationContainerWithSlotGameActionContext final : public IGameActionContext
{
public:
    TLocationContainerWithSlotGameActionContext(uint8_t containerId, int slot)
        : m_containerId(containerId)
        , m_slot(slot)
    {
    }
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT;
    }
    uint8_t containerId() const { return m_containerId; }
    int slot() const { return m_slot; }
    bool containerIsLootCorpse() const { return m_containerIsLootCorpse; }
    void setContainerIsLootCorpse(bool value) { m_containerIsLootCorpse = value; }
    bool containerIsBrowseField() const { return m_containerIsBrowseField; }
    void setContainerIsBrowseField(bool value) { m_containerIsBrowseField = value; }
    /// +0x12, read by the container handler's move (zeroed by the constructor;
    /// no writer identified yet).
    bool flag12() const { return m_flag12; }
    /// CIP compares the container and the slot only.
    bool operator==(const TLocationContainerWithSlotGameActionContext& other) const
    {
        return m_slot == other.m_slot && m_containerId == other.m_containerId;
    }

private:
    uint8_t m_containerId;
    int m_slot;
    bool m_containerIsLootCorpse = false;
    bool m_containerIsBrowseField = false;
    bool m_flag12 = false;
};

/// CIP input::TLocationBattleListGameActionContext.
class TLocationBattleListGameActionContext final : public IGameActionContext
{
public:
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_BATTLE_LIST; }
};

/// CIP input::TLocationWorldMapGameActionContext: somewhere on the world map.
class TLocationWorldMapGameActionContext final : public IGameActionContext
{
public:
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_WORLD_MAP; }
};

/// CIP input::TLocationNpcTradeGameActionContext: a good (+0x8) of the NPC
/// trade window; two contexts are equal when they hold the same good.
class TLocationNpcTradeGameActionContext final : public IGameActionContext
{
public:
    explicit TLocationNpcTradeGameActionContext(std::shared_ptr<const trade::TNPCTradeGood> pGood) : m_pGood(std::move(pGood)) {}
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_NPC_TRADE; }
    const std::shared_ptr<const trade::TNPCTradeGood>& good() const { return m_pGood; }
    bool operator==(const TLocationNpcTradeGameActionContext& other) const { return m_pGood == other.m_pGood; }

private:
    std::shared_ptr<const trade::TNPCTradeGood> m_pGood;
};

/// CIP input::TLocationUnknownContext: no location (an object hotkey).
class TLocationUnknownContext final : public IGameActionContext
{
public:
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_UNKNOWN; }
};

/// CIP input::TLocationWorldMapCoordinateContext: a field (+0x8).
class TLocationWorldMapCoordinateContext : public IGameActionContext
{
public:
    explicit TLocationWorldMapCoordinateContext(const Position& coordinate) : m_coordinate(coordinate) {}
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE;
    }
    const Position& coordinate() const { return m_coordinate; }
    bool operator==(const TLocationWorldMapCoordinateContext& other) const { return m_coordinate == other.m_coordinate; }

private:
    Position m_coordinate;
};

/// CIP input::TLocationWorldMapCoordinateWithStackPositionContext: an object
/// (stack position +0x20) on a field (+0x8).
class TLocationWorldMapCoordinateWithStackPositionContext final : public IGameActionContext
{
public:
    TLocationWorldMapCoordinateWithStackPositionContext(const Position& coordinate, uint8_t stackPosition)
        : m_coordinate(coordinate)
        , m_stackPosition(stackPosition)
    {
    }
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION;
    }
    const Position& coordinate() const { return m_coordinate; }
    uint8_t stackPosition() const { return m_stackPosition; }
    bool operator==(const TLocationWorldMapCoordinateWithStackPositionContext& other) const
    {
        return m_coordinate == other.m_coordinate && m_stackPosition == other.m_stackPosition;
    }

private:
    Position m_coordinate;
    uint8_t m_stackPosition;
};

/// CIP input::TLocationDepotSearchGameActionContext: an item (+0x8) of the depot
/// search's depot or inbox list.
class TLocationDepotSearchGameActionContext : public IGameActionContext
{
public:
    explicit TLocationDepotSearchGameActionContext(uint32_t index) : m_index(index) {}
    uint32_t index() const { return m_index; }

private:
    uint32_t m_index;
};

/// CIP input::TLocationDepotSearchDepotItemGameActionContext.
class TLocationDepotSearchDepotItemGameActionContext final : public TLocationDepotSearchGameActionContext
{
public:
    explicit TLocationDepotSearchDepotItemGameActionContext(uint32_t index) : TLocationDepotSearchGameActionContext(index) {}
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM;
    }
};

/// CIP input::TLocationDepotSearchInboxItemGameActionContext.
class TLocationDepotSearchInboxItemGameActionContext final : public TLocationDepotSearchGameActionContext
{
public:
    explicit TLocationDepotSearchInboxItemGameActionContext(uint32_t index) : TLocationDepotSearchGameActionContext(index) {}
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM;
    }
};

/// CIP input::TLocationActionButtonGameActionContext: a button (+0xc) of an
/// action bar (+0x8); +0x10 is the bar's third coordinate.
class TLocationActionButtonGameActionContext final : public IGameActionContext
{
public:
    TLocationActionButtonGameActionContext(uint8_t bar, int button, int value)
        : m_bar(bar)
        , m_button(button)
        , m_value(value)
    {
    }
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_ACTION_BUTTON; }
    uint8_t bar() const { return m_bar; }
    int button() const { return m_button; }
    int value() const { return m_value; }

private:
    uint8_t m_bar;
    int m_button;
    int m_value;
};

/// CIP input::TLocationPlayerTradeGameActionContext: an item (+0xc) of one side
/// (+0x8) of the player trade.
class TLocationPlayerTradeGameActionContext final : public IGameActionContext
{
public:
    TLocationPlayerTradeGameActionContext(int side, int index)
        : m_side(side)
        , m_index(index)
    {
    }
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::LOCATION_PLAYER_TRADE; }
    int side() const { return m_side; }
    int index() const { return m_index; }
    bool operator==(const TLocationPlayerTradeGameActionContext& other) const
    {
        return m_side == other.m_side && m_index == other.m_index;
    }

private:
    int m_side;
    int m_index;
};

/// CIP input::TLocationNPCChatKeywordGameActionContext: a keyword (+0x8) of an NPC's chat.
class TLocationNPCChatKeywordGameActionContext final : public IGameActionContext
{
public:
    explicit TLocationNPCChatKeywordGameActionContext(QString keyword) : m_keyword(std::move(keyword)) {}
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_NPC_CHAT_KEYWORD;
    }
    const QString& keyword() const { return m_keyword; }
    bool operator==(const TLocationNPCChatKeywordGameActionContext& other) const { return m_keyword == other.m_keyword; }

private:
    QString m_keyword;
};

/// CIP input::TLocationTutorialHintDialogGameActionContext.
class TLocationTutorialHintDialogGameActionContext final : public IGameActionContext
{
public:
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::LOCATION_TUTORIAL_HINT_DIALOG;
    }
};

// --- Objects ----------------------------------------------------------------

/// CIP's kinds of appearance type object context (TObjectApperanceTypeGameActionContext::v4).
enum class EObjectApperanceTypeContextType : int
{
    PLAIN = 0,
    LIQUID = 1,
    UPGRADEABLE = 2,
};

/// CIP input::TObjectApperanceTypeGameActionContext: an object of an
/// appearance type (+0x8).
class TObjectApperanceTypeGameActionContext : public IGameActionContext
{
public:
    explicit TObjectApperanceTypeGameActionContext(uint16_t appearanceTypeId) : m_appearanceTypeId(appearanceTypeId) {}
    EGameActionContextType gameActionContextType() const override
    {
        return EGameActionContextType::OBJECT_APPEARANCE_TYPE;
    }
    /// CIP v3.
    uint16_t appearanceTypeId() const { return m_appearanceTypeId; }
    /// CIP v4.
    virtual EObjectApperanceTypeContextType objectApperanceTypeContextType() const
    {
        return EObjectApperanceTypeContextType::PLAIN;
    }

private:
    uint16_t m_appearanceTypeId;
};

/// CIP input::TLiquidObjectApperanceTypeGameActionContext: a fluid container's
/// liquid (+0xc).
class TLiquidObjectApperanceTypeGameActionContext final : public TObjectApperanceTypeGameActionContext
{
public:
    TLiquidObjectApperanceTypeGameActionContext(uint16_t appearanceTypeId, uint32_t liquidType)
        : TObjectApperanceTypeGameActionContext(appearanceTypeId)
        , m_liquidType(liquidType)
    {
    }
    EObjectApperanceTypeContextType objectApperanceTypeContextType() const override
    {
        return EObjectApperanceTypeContextType::LIQUID;
    }
    /// CIP v5.
    uint32_t liquidType() const { return m_liquidType; }
    bool operator==(const TLiquidObjectApperanceTypeGameActionContext& other) const
    {
        return m_liquidType == other.m_liquidType && appearanceTypeId() == other.appearanceTypeId();
    }

private:
    uint32_t m_liquidType;
};

/// CIP input::TUpgradeableObjectApperanceTypeGameActionContext: a classified
/// object's tier (+0xa).
class TUpgradeableObjectApperanceTypeGameActionContext final : public TObjectApperanceTypeGameActionContext
{
public:
    TUpgradeableObjectApperanceTypeGameActionContext(uint16_t appearanceTypeId, uint8_t tier)
        : TObjectApperanceTypeGameActionContext(appearanceTypeId)
        , m_tier(tier)
    {
    }
    EObjectApperanceTypeContextType objectApperanceTypeContextType() const override
    {
        return EObjectApperanceTypeContextType::UPGRADEABLE;
    }
    /// CIP v5.
    uint8_t tier() const { return m_tier; }
    bool operator==(const TUpgradeableObjectApperanceTypeGameActionContext& other) const
    {
        return m_tier == other.m_tier && appearanceTypeId() == other.appearanceTypeId();
    }

private:
    uint8_t m_tier;
};

/// CIP input::TDecoItemKitObjectApperanceTypeGameActionContext: a decoration
/// kit's packed item (+0xa). Its v4 stays PLAIN.
class TDecoItemKitObjectApperanceTypeGameActionContext final : public TObjectApperanceTypeGameActionContext
{
public:
    TDecoItemKitObjectApperanceTypeGameActionContext(uint16_t appearanceTypeId, uint16_t decoItemId)
        : TObjectApperanceTypeGameActionContext(appearanceTypeId)
        , m_decoItemId(decoItemId)
    {
    }
    /// CIP v5.
    uint16_t decoItemId() const { return m_decoItemId; }
    bool operator==(const TDecoItemKitObjectApperanceTypeGameActionContext& other) const
    {
        return m_decoItemId == other.m_decoItemId && appearanceTypeId() == other.appearanceTypeId();
    }

private:
    uint16_t m_decoItemId;
};

/// CIP input::TObjectCreatureGameActionContext: a creature (+0x8).
class TObjectCreatureGameActionContext final : public IGameActionContext
{
public:
    explicit TObjectCreatureGameActionContext(uint32_t creatureId) : m_creatureId(creatureId) {}
    EGameActionContextType gameActionContextType() const override { return EGameActionContextType::OBJECT_CREATURE; }
    uint32_t creatureId() const { return m_creatureId; }
    bool operator==(const TObjectCreatureGameActionContext& other) const { return m_creatureId == other.m_creatureId; }

private:
    uint32_t m_creatureId;
};

// --- qtclient's protocol form -------------------------------------------------

/// The classic position's y of a depot search depot and inbox item; x is
/// 0xffff and z the item's index among the searched type's objects there.
constexpr uint16_t DepotSearchDepotItemY = 0x20;
constexpr uint16_t DepotSearchInboxItemY = 0x21;
/// The location of qtclient's classic object position: an inventory slot is
/// Position(0xFFFF, slot, 0), a container slot Position(0xFFFF, 0x40 | id, slot),
/// a map object its coordinate with the stack position (none: the field), a
/// depot search item Position(0xFFFF, 0x20 or 0x21, index);
/// Position(0xFFFF, 0, 0) is anywhere (TLocationUnknownContext). An invalid
/// position gives no location (null).
TGameActionContextPtr locationContextFromPosition(const Position& position, int stackPosition);
/// The classic object position of a location: Position(0xFFFF, 0, 0) for an
/// unknown one, (0xFFFF, 0x20 or 0x21, index) for a depot search item, invalid
/// for one without a field (battle list, NPC trade, ...).
Position protocolPosition(const IGameActionContext* location);
/// The classic stack position of a location: the map stack position, the
/// container slot, 0 for an inventory slot or an unknown location; -1 for any other.
int protocolStackPosition(const IGameActionContext* location);
/// The appearance type id of an object context (0 for a creature or none).
uint16_t appearanceTypeIdOf(const IGameActionContext* object);
/// The creature of an object context (0 for an appearance type or none).
uint32_t creatureIdOf(const IGameActionContext* object);

/// CIP: two appearance type objects are equal when they are the same kind, have the
/// same liquid or tier, and have the same appearance type. A deco kit counts as plain.
bool objectApperanceTypeContextsEqual(const TObjectApperanceTypeGameActionContext& first,
                                      const TObjectApperanceTypeGameActionContext& second);
/// CIP: two contexts are equal when they are the same kind with the same fields. A battle
/// list or world map location always equals another of its kind. An unknown, NPC trade,
/// player trade, action button or tutorial hint dialog location never does.
bool gameActionContextsEqual(const IGameActionContext& first, const IGameActionContext& second);
/// CIP: two contexts mean the same place or object when one of these holds:
/// - one is a field with a stack position, the other a field without one, on the same
///   coordinate;
/// - both are appearance type objects of the same type;
/// - they are equal (gameActionContextsEqual).
bool gameActionContextsMatch(const IGameActionContext& first, const IGameActionContext& second);

}
}
