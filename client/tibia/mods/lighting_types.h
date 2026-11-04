#pragma once

#include <cstdint>

namespace tibia::mods {

/// CIP 6³ palette byte → RGB (same formula as framework `Color::from8bit`).
/// Packs prefer this over `#include <framework/util/color.h>`.
struct ColorRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr float rF() const { return r / 255.f; }
    constexpr float gF() const { return g / 255.f; }
    constexpr float bF() const { return b / 255.f; }
};

inline constexpr ColorRgb colorFrom8bit(const uint8_t color, const float brightness = 1.0f)
{
    if (color >= 216 || color == 0)
        return {};
    return {
        static_cast<uint8_t>(static_cast<int>((color / 36 % 6 * 51) * brightness)),
        static_cast<uint8_t>(static_cast<int>((color / 6 % 6 * 51) * brightness)),
        static_cast<uint8_t>(static_cast<int>((color % 6 * 51) * brightness)),
    };
}

/// POD map cell for WorldLightingDriver / WorldGeometry (no tip `Position`).
struct LightingMapPos {
    int x = 0;
    int y = 0;
    int z = 0;

    friend bool operator==(const LightingMapPos& a, const LightingMapPos& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend bool operator!=(const LightingMapPos& a, const LightingMapPos& b) { return !(a == b); }
};

/// Peeled thing context for stamp / light gather (tip fills; pack never sees Thing&).
struct LightingThingContext {
    uint8_t lift = 0;
    bool creature = false;
    bool walking = false;
    bool localPlayer = false;
    bool loose = false;
    bool transient = false;
    bool caster = true;
};

/// Peeled frame desc for `addFrame` (tip peels ThingType; pack never sees ThingType&).
struct LightingFrameDesc {
    uint32_t spriteId = 0;
    int anchorX = 0;
    int anchorY = 0;
    int displacementX = 0;
    int displacementY = 0;
    uint8_t shape = 0; // ThingShape as uint8_t
    uint8_t elevation = 0;
    int8_t hook = -1; // hangable xPattern or -1
    bool onTop = false;
    bool castsShadow = false;
    bool lyingCorpse = false;
    bool hasLight = false;
    uint8_t lightIntensity = 0;
    uint8_t lightColor = 0;
};

} // namespace tibia::mods
