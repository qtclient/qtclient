#include "spritesession.h"

#include "sprites/spritesystem.h"

namespace tibia::sprites {

QImage spriteImage(const uint32_t id)
{
    bool loading = false;
    return TSpriteSystem::instance().spriteImage(id, loading);
}

QImage spriteImageDecoded(const uint32_t id, bool& loading)
{
    return TSpriteSystem::instance().spriteImageDecoded(id, loading);
}

} // namespace tibia::sprites
