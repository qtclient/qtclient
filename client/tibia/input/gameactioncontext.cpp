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

}
