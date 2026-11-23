#include "protocol/protobufobjectpositionhelper.h"

#include "protocol/objectpositionbuilder.h"

#include <protocol.pb.h>

namespace tibia::protocol {

using ::tibia::protobuf::protocol::ContainerObjectPosition;
using ::tibia::protobuf::protocol::ObjectPosition;

namespace {

constexpr int32_t NoField = 0xffff;
constexpr int32_t DepotSearchDepotY = 0x20;
constexpr int32_t DepotSearchInboxY = 0x21;
constexpr int32_t ContainerY = 0x40;
constexpr uint32_t InventorySlotEnd = 0xb;

}

Position TProtobufObjectPositionHelper::toClassicCoordinate(const ObjectPosition& objectPosition) const
{
    Position coordinate(0, 0, 0);
    switch (objectPosition.position_oneof_case()) {
    case ObjectPosition::kWorldmapPosition: {
        const auto& field = objectPosition.worldmap_position().coordinate();
        coordinate.x = static_cast<int32_t>(field.x());
        coordinate.y = static_cast<int32_t>(field.y());
        coordinate.z = static_cast<uint8_t>(field.z());
        break;
    }
    case ObjectPosition::kInventoryPosition:
        coordinate.x = NoField;
        coordinate.y = static_cast<int32_t>(objectPosition.inventory_position().slot());
        break;
    case ObjectPosition::kContainerPosition: {
        const auto& container = objectPosition.container_position();
        coordinate.x = NoField;
        coordinate.y = static_cast<int32_t>(container.container_id()) + ContainerY;
        switch (container.slot_oneof_case()) {
        case ContainerObjectPosition::kSpecialSlotFe:
            coordinate.z = static_cast<uint8_t>(container.special_slot_fe() ? 0xfe : 0);
            break;
        case ContainerObjectPosition::kSpecialSlotFf:
            coordinate.z = static_cast<uint8_t>(container.special_slot_ff() ? 0xff : 0);
            break;
        case ContainerObjectPosition::kSlot:
            coordinate.z = static_cast<uint8_t>(container.slot());
            break;
        default:
            coordinate.z = 0;
            break;
        }
        break;
    }
    case ObjectPosition::kUnknownPosition:
        if (objectPosition.unknown_position()) {
            coordinate.x = NoField;
            coordinate.z = 0;
        }
        break;
    case ObjectPosition::kDepotSearchInboxPosition:
        coordinate.x = NoField;
        coordinate.y = DepotSearchInboxY;
        coordinate.z = static_cast<uint8_t>(objectPosition.depot_search_inbox_position().index());
        break;
    case ObjectPosition::kDepotSearchDepotPosition:
        coordinate.x = NoField;
        coordinate.y = DepotSearchDepotY;
        coordinate.z = static_cast<uint8_t>(objectPosition.depot_search_depot_position().index());
        break;
    default:
        break;
    }
    return coordinate;
}

void TProtobufObjectPositionHelper::fromClassicCoordinate(ObjectPosition* objectPosition, const Position& coordinate) const
{
    TObjectPositionBuilder builder(objectPosition);
    if (coordinate.x != NoField) {
        builder.setWorldmapCoordinate(coordinate);
        return;
    }
    const int32_t y = coordinate.y;
    if (y == 0) {
        objectPosition->set_unknown_position(true);
    } else if (static_cast<uint32_t>(y) < InventorySlotEnd) {
        builder.setInventorySlot(y);
    } else if (y == DepotSearchInboxY) {
        builder.setDepotSearchInboxIndex(coordinate.z);
    } else if (y == DepotSearchDepotY) {
        builder.setDepotSearchDepotIndex(coordinate.z);
    } else if (y >= ContainerY) {
        builder.setContainerId(static_cast<uint8_t>(y - ContainerY)).setContainerSlot(coordinate.z);
    }
}

}
