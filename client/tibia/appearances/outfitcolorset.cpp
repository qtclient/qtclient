#include "appearances/outfitcolorset.h"

#include "colorhelper.h"

#include <protocol.pb.h>

namespace tibia::appearances {

namespace {

uint8_t paletteIndex(const uint32_t index)
{
    return tibia::shared::outfitColorIndex(tibia::shared::outfitColor(static_cast<uint8_t>(index)));
}

}

TOutfitColorSet outfitColorsFromProtobuf(const protobuf::protocol::OutfitColor& colors)
{
    TOutfitColorSet set;
    set.head = paletteIndex(colors.head());
    set.body = paletteIndex(colors.body());
    set.legs = paletteIndex(colors.legs());
    set.feet = paletteIndex(colors.feet());
    return set;
}

}
