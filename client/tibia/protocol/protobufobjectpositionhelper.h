#pragma once

#include "client/position.h"

namespace tibia::protobuf::protocol {
class ObjectPosition;
}

namespace tibia::protocol {

/// CIP protocol::TProtobufObjectPositionHelper: converts between a client
/// message's ObjectPosition and the classic wire coordinate. Classic: a map
/// field is its coordinate; anything else has x = 0xffff and y = the inventory
/// slot (1..10), 0x20 / 0x21 for a depot search depot / inbox item (z = index),
/// 0x40 + container id for a container slot (z = slot), or 0 for "anywhere".
class TProtobufObjectPositionHelper
{
public:
    virtual ~TProtobufObjectPositionHelper() = default;

    /// The classic coordinate of a position; (0, 0, 0) when no case is set.
    virtual Position toClassicCoordinate(const ::tibia::protobuf::protocol::ObjectPosition& objectPosition) const;
    /// The position of a classic coordinate. A container slot always becomes a
    /// plain slot, also 0xfe and 0xff; unassigned y values leave it untouched.
    virtual void fromClassicCoordinate(::tibia::protobuf::protocol::ObjectPosition* objectPosition,
                                       const Position& coordinate) const;
};

}
