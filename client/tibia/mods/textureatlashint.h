#pragma once

// Host: an extension's hint for the scene graph's texture atlas (larger sprite
// textures, e.g. upscaled art). No CipSoft counterpart; without a hint the
// atlas keeps CipSoft's defaults.

#include "qtclient/abi/registry.h"

namespace tibia::mods {

/// Host ABI `set_texture_atlas_hint`: `fn` is asked once, when the application
/// sets up graphics, before any window exists. Null removes it.
void setTextureAtlasHint(QtClientTextureAtlasHintFn fn);
/// `defaultSide` (the atlas side when the client settings store none) and
/// `sizeLimitCap` (the largest texture side that goes into the atlas) as the
/// hint changes them; unchanged without one.
void applyTextureAtlasHint(int& defaultSide, int& sizeLimitCap);

}
