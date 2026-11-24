#pragma once

#include "qtclient/abi/lighting.h"

#include <cstdint>

namespace tibia {
namespace mods {

/// POD map cell for WorldLightingDriver (no qtclient `Position`).
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

/// What the frame build draws next (qtclient fills; the driver never sees Thing&). Only CipSoft
/// data: what the driver makes of it (who casts a shadow) is the driver's.
struct LightingThingContext {
    uint8_t lift = 0;
    bool creature = false;
    bool walking = false;
    bool localPlayer = false;
    /// An object without CipSoft's unmove flag.
    bool loose = false;
    /// An effect or a missile.
    bool transient = false;
    /// Stable creature id for stamp/light identity (0 when not a creature).
    uint32_t creatureId = 0;
    /// The drawn instance's type (0 for a creature or a transient) and its
    /// QTCLIENT_APPEARANCE_* flags.
    uint16_t typeId = 0;
    uint32_t typeFlags = 0;
    /// Position in the stack of the field LightingField last named; -1 when the
    /// thing is drawn on a field it does not stand on (a stepping creature).
    int16_t stackIndex = -1;
};

/// Peeled frame desc for `addFrame` (qtclient peels TAppearanceType; the driver never sees TAppearanceType&).
struct LightingFrameDesc {
    uint32_t spriteId = 0;
    int anchorX = 0;
    int anchorY = 0;
    int displacementX = 0;
    int displacementY = 0;
    uint8_t elevation = 0;
    int8_t hook = -1; // hangable xPattern or -1
    bool onTop = false;
    /// CipSoft lying object, corpse or player corpse.
    bool lyingCorpse = false;
    bool hasLight = false;
    uint8_t lightIntensity = 0;
    uint8_t lightColor = 0;
    /// The frame's type and its QTCLIENT_APPEARANCE_* flags.
    uint16_t typeId = 0;
    uint32_t typeFlags = 0;
};

/// The field the frame build draws next: its draw-grid cell and its stack
/// (CipSoft order; borrowed for the call).
struct LightingField {
    int ix = 0;
    int iy = 0;
    LightingMapPos pos;
    const QtClientFieldStackEntry* stack = nullptr;
    int stackCount = 0;
};

}
}
