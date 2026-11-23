#include "gameactioncontext.h"

namespace tibia::input {

TGameActionContextPtr locationContextFromPosition(const Position& position, const int stackPosition)
{
    if (!position.isValid())
        return nullptr;
    if (position.x != UINT16_MAX) {
        if (stackPosition < 0)
            return std::make_shared<TLocationWorldMapCoordinateContext>(position);
        return std::make_shared<TLocationWorldMapCoordinateWithStackPositionContext>(
            position, static_cast<uint8_t>(stackPosition));
    }
    if (position.y & 0x40)
        return std::make_shared<TLocationContainerWithSlotGameActionContext>(static_cast<uint8_t>(position.y & 0x3f),
                                                                             position.z);
    if (position.y == 0)
        return std::make_shared<TLocationUnknownContext>();
    if (position.y == DepotSearchDepotItemY)
        return std::make_shared<TLocationDepotSearchDepotItemGameActionContext>(position.z);
    if (position.y == DepotSearchInboxItemY)
        return std::make_shared<TLocationDepotSearchInboxItemGameActionContext>(position.z);
    return std::make_shared<TLocationInventoryGameActionContext>(position.y);
}

Position protocolPosition(const IGameActionContext* location)
{
    if (!location)
        return {};
    switch (location->gameActionContextType()) {
    case EGameActionContextType::LOCATION_INVENTORY:
        return { UINT16_MAX, static_cast<const TLocationInventoryGameActionContext*>(location)->slot(), 0 };
    case EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT: {
        const auto* container = static_cast<const TLocationContainerWithSlotGameActionContext*>(location);
        return { UINT16_MAX, 0x40 | container->containerId(), static_cast<uint8_t>(container->slot()) };
    }
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE:
        return static_cast<const TLocationWorldMapCoordinateContext*>(location)->coordinate();
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
        return static_cast<const TLocationWorldMapCoordinateWithStackPositionContext*>(location)->coordinate();
    case EGameActionContextType::LOCATION_UNKNOWN:
        // Anywhere the player has one (object hotkeys): the classic use by type.
        return { UINT16_MAX, 0, 0 };
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM:
        // The searched type's n-th object in the depot or the inbox.
        return { UINT16_MAX, DepotSearchDepotItemY,
                 static_cast<uint8_t>(static_cast<const TLocationDepotSearchGameActionContext*>(location)->index()) };
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM:
        return { UINT16_MAX, DepotSearchInboxItemY,
                 static_cast<uint8_t>(static_cast<const TLocationDepotSearchGameActionContext*>(location)->index()) };
    default:
        return {};
    }
}

int protocolStackPosition(const IGameActionContext* location)
{
    if (!location)
        return -1;
    switch (location->gameActionContextType()) {
    case EGameActionContextType::LOCATION_INVENTORY:
    case EGameActionContextType::LOCATION_UNKNOWN:
        return 0;
    case EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT:
        return static_cast<const TLocationContainerWithSlotGameActionContext*>(location)->slot();
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
        return static_cast<const TLocationWorldMapCoordinateWithStackPositionContext*>(location)->stackPosition();
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM:
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM:
        return static_cast<int>(static_cast<const TLocationDepotSearchGameActionContext*>(location)->index());
    default:
        return -1;
    }
}

uint16_t appearanceTypeIdOf(const IGameActionContext* object)
{
    if (const auto* type = dynamic_cast<const TObjectApperanceTypeGameActionContext*>(object))
        return type->appearanceTypeId();
    return 0;
}

uint32_t creatureIdOf(const IGameActionContext* object)
{
    if (const auto* creature = dynamic_cast<const TObjectCreatureGameActionContext*>(object))
        return creature->creatureId();
    return 0;
}

bool objectApperanceTypeContextsEqual(const TObjectApperanceTypeGameActionContext& first,
                                      const TObjectApperanceTypeGameActionContext& second)
{
    const EObjectApperanceTypeContextType kind = first.objectApperanceTypeContextType();
    if (kind != second.objectApperanceTypeContextType())
        return false;
    if (kind != EObjectApperanceTypeContextType::PLAIN) {
        if (kind == EObjectApperanceTypeContextType::UPGRADEABLE) {
            const auto* left = dynamic_cast<const TUpgradeableObjectApperanceTypeGameActionContext*>(&first);
            const auto* right = dynamic_cast<const TUpgradeableObjectApperanceTypeGameActionContext*>(&second);
            if (!left || !right || left->tier() != right->tier())
                return false;
        } else if (kind == EObjectApperanceTypeContextType::LIQUID) {
            const auto* left = dynamic_cast<const TLiquidObjectApperanceTypeGameActionContext*>(&first);
            const auto* right = dynamic_cast<const TLiquidObjectApperanceTypeGameActionContext*>(&second);
            if (!left || !right || left->liquidType() != right->liquidType())
                return false;
        } else {
            return false;
        }
        if (first.gameActionContextType() != second.gameActionContextType())
            return false;
    }
    return first.appearanceTypeId() == second.appearanceTypeId();
}

namespace {

template<typename T, typename F>
bool contextFieldsEqual(const IGameActionContext& first, const IGameActionContext& second, F equal)
{
    const auto* left = dynamic_cast<const T*>(&first);
    const auto* right = dynamic_cast<const T*>(&second);
    return left && right && equal(*left, *right);
}

}

bool gameActionContextsEqual(const IGameActionContext& first, const IGameActionContext& second)
{
    if (first.gameActionContextType() != second.gameActionContextType())
        return false;
    switch (first.gameActionContextType()) {
    case EGameActionContextType::LOCATION_INVENTORY:
        return contextFieldsEqual<TLocationInventoryGameActionContext>(
            first, second, [](const auto& left, const auto& right) { return left.slot() == right.slot(); });
    case EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT:
        return contextFieldsEqual<TLocationContainerWithSlotGameActionContext>(
            first, second, [](const auto& left, const auto& right) {
                return left.slot() == right.slot() && left.containerId() == right.containerId();
            });
    case EGameActionContextType::LOCATION_BATTLE_LIST:
    case EGameActionContextType::LOCATION_WORLD_MAP:
        return true;
    case EGameActionContextType::LOCATION_NPC_CHAT_KEYWORD:
        return contextFieldsEqual<TLocationNPCChatKeywordGameActionContext>(
            first, second, [](const auto& left, const auto& right) { return left.keyword() == right.keyword(); });
    case EGameActionContextType::OBJECT_APPEARANCE_TYPE:
        return contextFieldsEqual<TObjectApperanceTypeGameActionContext>(first, second, &objectApperanceTypeContextsEqual);
    case EGameActionContextType::OBJECT_CREATURE:
        return contextFieldsEqual<TObjectCreatureGameActionContext>(
            first, second, [](const auto& left, const auto& right) { return left.creatureId() == right.creatureId(); });
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE:
        return contextFieldsEqual<TLocationWorldMapCoordinateContext>(
            first, second, [](const auto& left, const auto& right) { return left.coordinate() == right.coordinate(); });
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
        return contextFieldsEqual<TLocationWorldMapCoordinateWithStackPositionContext>(
            first, second, [](const auto& left, const auto& right) {
                return left.stackPosition() == right.stackPosition() && left.coordinate() == right.coordinate();
            });
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM:
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM:
        return contextFieldsEqual<TLocationDepotSearchGameActionContext>(
            first, second, [](const auto& left, const auto& right) { return left.index() == right.index(); });
    default:
        return false;
    }
}

bool gameActionContextsMatch(const IGameActionContext& first, const IGameActionContext& second)
{
    using E = EGameActionContextType;
    const E firstType = first.gameActionContextType();
    const E secondType = second.gameActionContextType();
    // The kinds that can mean the same thing: a field with or without a stack
    // position, and two appearance type objects.
    const auto isField = [](const E type) {
        return type == E::LOCATION_WORLD_MAP_COORDINATE || type == E::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION;
    };
    const bool sameGroup = (isField(firstType) && isField(secondType))
        || (firstType == E::OBJECT_APPEARANCE_TYPE && secondType == E::OBJECT_APPEARANCE_TYPE);
    bool matched = false;
    if (sameGroup) {
        if (firstType == E::OBJECT_APPEARANCE_TYPE) {
            matched = appearanceTypeIdOf(&first) == appearanceTypeIdOf(&second);
        } else if (firstType != secondType) {
            const auto coordinateOf = [](const IGameActionContext& context) {
                if (const auto* field = dynamic_cast<const TLocationWorldMapCoordinateContext*>(&context))
                    return field->coordinate();
                return static_cast<const TLocationWorldMapCoordinateWithStackPositionContext&>(context).coordinate();
            };
            matched = coordinateOf(first) == coordinateOf(second);
        }
    }
    if (!matched && firstType == secondType)
        return gameActionContextsEqual(first, second);
    return matched;
}

}
