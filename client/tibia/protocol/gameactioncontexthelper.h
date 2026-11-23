#pragma once

#include "input/gameactioncontext.h"

namespace tibia::protobuf::protocol {
class ObjectIdentifier;
class ObjectPosition;
}

namespace tibia::protocol {

/// CIP protocol::TGameActionContextHelper: writes the location and the object
/// of a game action into the ObjectPosition and ObjectIdentifier of a client
/// message.
class TGameActionContextHelper
{
public:
    virtual ~TGameActionContextHelper() = default;

    /// The location: a map field (with the object's stack position, if any), an
    /// inventory slot, a container slot, a depot search item or "anywhere"
    /// (unknown location). Other locations leave the position empty.
    virtual void fillObjectPosition(const input::TGameActionContextPtr& location,
                                    ::tibia::protobuf::protocol::ObjectPosition* objectPosition) const;
    /// The object of an appearance type: its type id, and the liquid of a fluid
    /// or the tier of a classified object. Other objects leave it untouched.
    virtual void fillObjectIdentifier(const input::TGameActionContextPtr& object,
                                      ::tibia::protobuf::protocol::ObjectIdentifier* objectIdentifier) const;
};

}
