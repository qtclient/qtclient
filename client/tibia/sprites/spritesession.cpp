#include "spritesession.h"

#include <tibia/assets/spritesheet.h>

namespace tibia::sprites {

QImage spriteImage(const uint32_t id)
{
    bool loading = false;
    return tibia::assets::sprites().spriteImage(static_cast<int>(id), loading);
}

} // namespace tibia::sprites
