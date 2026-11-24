#pragma once

/// Host extension point: the effect drawn for a server magic effect id.
/// Without a hook every id draws itself (CipSoft's behaviour).

#include <cstdint>

namespace tibia::effects {

/// What an extension draws for one server effect id.
struct TEffectRemap {
    /// The effect appearance id drawn (ThingCategoryEffect).
    uint16_t drawId = 0;
    /// Recolour key handed to the image tint hook when the sprite texture is made; 0 none.
    uint8_t tint = 0;
    /// Drawn in the creature layer of its field instead of on top of it.
    bool underCreatures = false;
};

/// True when the hook remaps `effectId` (filling `out`); false leaves the id as it is.
using EffectRemapFn = bool (*)(void* user, uint16_t effectId, TEffectRemap& out);

void setEffectRemapHook(EffectRemapFn fn, void* user);

/// The effect drawn for server effect `effectId`: the hook's remap, or the id itself.
TEffectRemap remapEffect(uint16_t effectId);

}
