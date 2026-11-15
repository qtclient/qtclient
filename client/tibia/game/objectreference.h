#pragma once

#include "client/position.h"

#include <cstdint>

namespace tibia {
namespace game {

/// An object the player can act on: its type, where it is (a map field, or the
/// inventory / a container as 0xFFFF positions) and the protocol details a
/// client message needs.
struct TObjectReference
{
    uint16_t typeId = 0;
    Position position;
    int stackPos = 0;
    uint16_t count = 0;
    uint8_t tier = 0;
    uint8_t subtype = 0;
};

}
}
