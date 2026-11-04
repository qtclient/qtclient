#include "spritesession.h"

#include <framework/graphics/image.h>
#include <tibia/assets/spritesheet.h>

namespace tibia::sprites {

QImage spriteImage(const uint32_t id)
{
    bool loading = false;
    const ImagePtr image = tibia::assets::sprites().spriteImage(static_cast<int>(id), loading);
    if (!image || image->getBpp() != 4)
        return {};
    const Size& size = image->getSize();
    return QImage(image->getPixelData(), size.width(), size.height(), size.width() * 4,
                  QImage::Format_RGBA8888)
        .copy();
}

} // namespace tibia::sprites
