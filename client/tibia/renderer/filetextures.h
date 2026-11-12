#pragma once

/// Tip façade for OTC file-texture cache lifecycle (Wave 2 hub peel).
/// Prefer tip QSG / TSpriteTextureCache for sprite emit. Do not invent a fake CIP T* name.
/// Bridges engine TextureManager terminate so tip product code stays off g_textures.

namespace tibia {
namespace renderer {

/// Drop OTC TextureManager cache before static teardown (avoids ResourceRegistry races).
void terminateFileTextures();

}
}
