#pragma once

#include <cstdint>

namespace tibia {
namespace mods {

/// POD map cell for WorldLightingDriver / WorldGeometry (no qtclient `Position`).
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

/// Peeled thing context for stamp / light gather (qtclient fills; pack never sees Thing&).
struct LightingThingContext {
    uint8_t lift = 0;
    bool creature = false;
    bool walking = false;
    bool localPlayer = false;
    bool loose = false;
    bool transient = false;
    bool caster = true;
    /// Stable creature id for stamp/light identity (0 when not a creature).
    uint32_t creatureId = 0;
};

/// Peeled frame desc for `addFrame` (qtclient peels ThingType; pack never sees ThingType&).
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

}
}
