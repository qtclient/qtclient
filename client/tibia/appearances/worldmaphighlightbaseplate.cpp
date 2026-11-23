#include "appearances/worldmaphighlightbaseplate.h"


#include <QFile>
#include <QImage>
#include <QtGlobal>

#include <algorithm>

namespace tibia::appearances {

std::shared_ptr<sprites::ISpriteIdentifier> TWorldmapHighlightBaseplateSpriteInfo::clone() const
{
    return std::make_shared<TWorldmapHighlightBaseplateSpriteInfo>(m_frame);
}

bool TWorldmapHighlightBaseplateSpriteInfo::equals(const sprites::ISpriteIdentifier& other) const
{
    return m_frame == static_cast<const TWorldmapHighlightBaseplateSpriteInfo&>(other).m_frame;
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
        QFile file(QStringLiteral(":/cursors/highlight-baseplate.png"));
        if (!file.open(QIODevice::ReadOnly))
            return QImage();
        QImage image;
        if (!image.load(&file, "PNG"))
            return QImage();
        return image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    }();
    if (strip.isNull() || strip.width() < FrameSize || strip.height() < FrameSize)
        return {};
    // The frame's 32-pixel cell, wrapped around the strip's width.
    const int left = (static_cast<int>(frame) * FrameSize) % strip.width();
    return strip.copy(left, 0, FrameSize, FrameSize);
}

}
