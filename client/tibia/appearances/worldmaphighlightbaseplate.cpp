#include "appearances/worldmaphighlightbaseplate.h"

#include "sprites/sprite.h"

#include <QFile>
#include <QImage>
#include <QtGlobal>

#include <algorithm>

namespace tibia::appearances {

SpriteKey TWorldmapHighlightBaseplateSpriteInfo::key() const
{
    return SpriteKey{ .highlightBaseplate = static_cast<int8_t>(m_frame) };
}

QImage TWorldmapHighlightBaseplateSpriteInfo::raster() const
{
    return imageForFrame(m_frame);
}

uint8_t TWorldmapHighlightBaseplateSpriteInfo::frameAtMillis(const qint64 millis)
{
    // CIP TWorldMapRenderProvider::v21:
    //   SEXT816(ms << 4) * 0x20c49ba5e353f7cf → (hi32 >> 7) - sign & 7
    // Equivalent integer form: floor(ms / 62.5) & 7 == ((ms * 2) / 125) & 7.
    // Period 500 ms across 8 frames (dwell alternates 62/63 ms).
    const qint64 ms = std::max<qint64>(0, millis);
    return static_cast<uint8_t>(((ms * 2) / 125) & 7);
}

QImage TWorldmapHighlightBaseplateSpriteInfo::imageForFrame(const uint8_t frame)
{
    static const QImage strip = [] {
        const sprites::TImageResourceSpriteIdentifier id(
            std::string(":/cursors/highlight-baseplate.png"));
        QFile file(QString::fromStdString(id.resourcePath()));
        if (!file.open(QIODevice::ReadOnly))
            return QImage();
        QImage image;
        if (!image.load(&file, "PNG"))
            return QImage();
        return image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    }();
    if (strip.isNull() || strip.width() < FrameCount * FrameSize || strip.height() < FrameSize)
        return {};
    const int index = static_cast<int>(frame % FrameCount);
    return strip.copy(index * FrameSize, 0, FrameSize, FrameSize);
}

}
