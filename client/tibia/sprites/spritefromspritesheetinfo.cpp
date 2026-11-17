#include "sprites/spritefromspritesheetinfo.h"

namespace tibia::sprites {

QRect TSpriteFromSpritesheetInfo::cellInSpritesheet(const TSpritesheetMetadata& metadata, const uint32_t spriteId)
{
    const QSize size = spriteSizeOfLayout(metadata.getLayout());
    const uint32_t columns = 384u / static_cast<uint32_t>(size.width());
    const uint32_t index = spriteId - metadata.getFirstSpriteId();
    const int x = static_cast<int>(index % columns) * size.width();
    const int y = static_cast<int>(index / columns) * size.height();
    return QRect(QPoint(x, y), QPoint(x + size.width() - 1, y + size.height() - 1));
}

}
