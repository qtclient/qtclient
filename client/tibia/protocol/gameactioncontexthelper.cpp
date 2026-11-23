#include "protocol/gameactioncontexthelper.h"

#include "protocol/objectpositionbuilder.h"

#include <protocol.pb.h>

namespace tibia::protocol {

using input::EGameActionContextType;
using input::EObjectApperanceTypeContextType;

void TGameActionContextHelper::fillObjectPosition(const input::TGameActionContextPtr& location,
                                                  ::tibia::protobuf::protocol::ObjectPosition* objectPosition) const
{
    switch (location->gameActionContextType()) {
    case EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT: {
        const auto* container = input::gameActionContextAs<input::TLocationContainerWithSlotGameActionContext>(location);
        TObjectPositionBuilder builder(objectPosition);
        builder.setContainerId(container->containerId());
        if (container->slot() == 0xff)
            builder.setContainerSpecialSlotFf();
        else if (container->slot() == 0xfe)
            builder.setContainerSpecialSlotFe();
        else
            builder.setContainerSlot(container->slot());
        break;
    }
    case EGameActionContextType::LOCATION_INVENTORY: {
        const auto* inventory = input::gameActionContextAs<input::TLocationInventoryGameActionContext>(location);
        TObjectPositionBuilder(objectPosition).setInventorySlot(inventory->slot());
        break;
    }
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION: {
        const auto* object =
            input::gameActionContextAs<input::TLocationWorldMapCoordinateWithStackPositionContext>(location);
        TObjectPositionBuilder(objectPosition)
            .setWorldmapCoordinate(object->coordinate())
            .setWorldmapStackPosition(object->stackPosition());
        break;
    }
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE: {
        const auto* field = input::gameActionContextAs<input::TLocationWorldMapCoordinateContext>(location);
        TObjectPositionBuilder(objectPosition).setWorldmapCoordinate(field->coordinate());
        break;
    }
    case EGameActionContextType::LOCATION_UNKNOWN:
        objectPosition->set_unknown_position(true);
        break;
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM: {
        const auto* item = input::gameActionContextAs<input::TLocationDepotSearchGameActionContext>(location);
        TObjectPositionBuilder(objectPosition).setDepotSearchDepotIndex(item->index());
        break;
    }
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM: {
        const auto* item = input::gameActionContextAs<input::TLocationDepotSearchGameActionContext>(location);
        TObjectPositionBuilder(objectPosition).setDepotSearchInboxIndex(item->index());
        break;
    }
    default:
        break;
    }
}

void TGameActionContextHelper::fillObjectIdentifier(const input::TGameActionContextPtr& object,
                                                    ::tibia::protobuf::protocol::ObjectIdentifier* objectIdentifier) const
{
    if (object->gameActionContextType() != EGameActionContextType::OBJECT_APPEARANCE_TYPE)
        return;
    const auto* type = input::gameActionContextAs<input::TObjectApperanceTypeGameActionContext>(object);
    objectIdentifier->set_type_id(type->appearanceTypeId());
    switch (type->objectApperanceTypeContextType()) {
    case EObjectApperanceTypeContextType::LIQUID:
        objectIdentifier->set_data(
            input::gameActionContextAs<input::TLiquidObjectApperanceTypeGameActionContext>(object)->liquidType());
        break;
    case EObjectApperanceTypeContextType::UPGRADEABLE:
        objectIdentifier->set_data(
            input::gameActionContextAs<input::TUpgradeableObjectApperanceTypeGameActionContext>(object)->tier());
        break;
    default:
        break;
    }
}

}
