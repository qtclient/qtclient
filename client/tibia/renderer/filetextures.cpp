#include "renderer/filetextures.h"

#include <framework/graphics/texturemanager.h>

namespace tibia {
namespace renderer {

void terminateFileTextures()
{
    g_textures.terminate();
}

}
}
