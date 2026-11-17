#pragma once

#include "sprites/spritesheetcache.h"

#include <QObject>

#include <memory>

namespace tibia::shared {
class TReadOnlyMemoryBlob;
}

namespace tibia {
namespace sprites {

/// Turns a spritesheet file's bytes into its image (CIP sprites::TSpritesheetDecompressor).
/// Also decodes other asset images (store images) the same way.
class TSpritesheetDecompressor : public QObject
{
    Q_OBJECT
public:
    explicit TSpritesheetDecompressor(QObject* parent = nullptr);

    /// Decodes `data`; decompressingSpritesheetFinished reports the image (null or
    /// a null image when the data is not an image).
    virtual void decompress(const TSpritesheetID& spritesheetId,
                            const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data) = 0;

    /// The image a spritesheet file holds, premultiplied ARGB32.
    static TSpritesheetPointer decode(const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data);

Q_SIGNALS:
    void decompressingSpritesheetFinished(const tibia::sprites::TSpritesheetID& SpritesheetID,
                                          const tibia::sprites::TSpritesheetPointer& pDecompressedSpritesheet);
};

}
}
