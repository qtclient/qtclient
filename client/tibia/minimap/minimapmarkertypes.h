#pragma once

#include <cstdint>
#include <string>

namespace tibia {
namespace minimap {

/// Static map annotation (stairs, levitate spots, NPCs, …) drawn on the minimap.
/// icon is the CIP map flag index (0..19, /images/game/minimap/flag<icon>).
struct MinimapMarker
{
    uint16_t x{ 0 };
    uint16_t y{ 0 };
    uint8_t icon{ 0 };
    std::string description;
};

constexpr uint8_t MINIMAP_MARKER_ICONS = 20;
/// Floors 0..15 inclusive (CIP map max Z).
constexpr int MINIMAP_MARKER_FLOORS = 16;

}
}
