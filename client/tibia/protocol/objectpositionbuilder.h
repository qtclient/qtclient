#pragma once

// CipSoft's chained writer of a client message's ObjectPosition: every call
// selects one case of the position oneof (keeping it when it is already the
// selected one) and sets a field of it.

#include "client/position.h"

#include <protocol.pb.h>

#include <cstdint>

namespace tibia::protocol {

class TObjectPositionBuilder
{
public:
    explicit TObjectPositionBuilder(::tibia::protobuf::protocol::ObjectPosition* objectPosition)
        : m_objectPosition(objectPosition)
    {
    }

    /// A map field.
    TObjectPositionBuilder& setWorldmapCoordinate(const Position& coordinate)
    {
        auto* field = m_objectPosition->mutable_worldmap_position()->mutable_coordinate();
        field->set_x(static_cast<uint32_t>(coordinate.x));
        field->set_y(static_cast<uint32_t>(coordinate.y));
        field->set_z(coordinate.z);
        return *this;
    }

    /// An object of that map field.
    TObjectPositionBuilder& setWorldmapStackPosition(const uint8_t stackPosition)
    {
        m_objectPosition->mutable_worldmap_position()->set_stack_position(stackPosition);
        return *this;
    }

    TObjectPositionBuilder& setInventorySlot(const int slot)
    {
        m_objectPosition->mutable_inventory_position()->set_slot(static_cast<uint32_t>(slot));
        return *this;
    }

    TObjectPositionBuilder& setContainerId(const uint8_t containerId)
    {
        m_objectPosition->mutable_container_position()->set_container_id(containerId);
        return *this;
    }

    TObjectPositionBuilder& setContainerSlot(const int slot)
    {
        m_objectPosition->mutable_container_position()->set_slot(static_cast<uint32_t>(slot));
        return *this;
    }

    /// The container's classic slot 0xfe.
    TObjectPositionBuilder& setContainerSpecialSlotFe()
    {
        m_objectPosition->mutable_container_position()->set_special_slot_fe(true);
        return *this;
    }

    /// The container's classic slot 0xff.
    TObjectPositionBuilder& setContainerSpecialSlotFf()
    {
        m_objectPosition->mutable_container_position()->set_special_slot_ff(true);
        return *this;
    }

    TObjectPositionBuilder& setDepotSearchDepotIndex(const uint32_t index)
    {
        m_objectPosition->mutable_depot_search_depot_position()->set_index(index);
        return *this;
    }

    TObjectPositionBuilder& setDepotSearchInboxIndex(const uint32_t index)
    {
        m_objectPosition->mutable_depot_search_inbox_position()->set_index(index);
        return *this;
    }

private:
    ::tibia::protobuf::protocol::ObjectPosition* m_objectPosition;
};

}
