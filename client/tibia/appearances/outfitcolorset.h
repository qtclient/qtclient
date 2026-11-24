#pragma once

#include <cstdint>

namespace tibia {
namespace protobuf::protocol {
class OutfitColor;
}
namespace appearances {

/// The four outfit palette indices of an outfit: head, body, legs and feet.
struct TOutfitColorSet
{
    uint8_t head = 0;
    uint8_t body = 0;
    uint8_t legs = 0;
    uint8_t feet = 0;

    /// All four indices are equal, compared in order.
    bool operator==(const TOutfitColorSet& other) const
    {
        return head == other.head && body == other.body && legs == other.legs && feet == other.feet;
    }
    bool operator!=(const TOutfitColorSet& other) const { return !(*this == other); }
};

/// The colour set of a protocol outfit colour message: each index (its low
/// byte) is taken through its palette colour, so an index past the palette
/// becomes the first index of that colour.
TOutfitColorSet outfitColorsFromProtobuf(const protobuf::protocol::OutfitColor& colors);

}
}
