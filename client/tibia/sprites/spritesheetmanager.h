#pragma once

#include "sprites/spritesheetcache.h"

#include <QMap>
#include <QMutex>
#include <QObject>

#include <memory>

namespace tibia::shared {
class TReadOnlyMemoryBlob;
}

namespace tibia {
namespace sprites {

/// A spritesheet's compressed file content.
using TCompressedSpritesheetPointer = std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>;

/// Registers TSpritesheetID, TSpritesheetPointer and TCompressedSpritesheetPointer
/// as meta types (they travel through queued signals).
void registerSpritesheetMetaTypes();

class TSpritesheetDecompressor;

/// Holds every spritesheet file's bytes and hands out their images, decompressing
/// a spritesheet the first time it is asked for after it left the cache
/// (CIP sprites::TSpritesheetManager). Thread safe: the renderer asks from its thread.
class TSpritesheetManager : public QObject
{
    Q_OBJECT
public:
    TSpritesheetManager(std::shared_ptr<TSpritesheetDecompressor> decompressor, std::shared_ptr<TSpritesheetCache> cache);
    ~TSpritesheetManager() override;

    /// A spritesheet file's bytes became available.
    virtual void addCompressedSpritesheet(const TSpritesheetID& spritesheetId,
                                          const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data);
    /// Caches a decompressed spritesheet; its bytes can be decompressed again later.
    virtual void addDecompressedSpritesheet(const TSpritesheetID& spritesheetId, const TSpritesheetPointer& spritesheet);
    /// The spritesheet's image, or null while it is decompressed (or not loaded:
    /// spritesheetMissing). A synchronous decompressor answers at once.
    virtual TSpritesheetPointer getSpritesheet(const TSpritesheetID& spritesheetId);

    /// Host: the spritesheet is decompressed and cached (no lookup side effects).
    bool isSpritesheetCached(const TSpritesheetID& spritesheetId) const;
    void clear();

    /// Host (pack lighting worker threads): the cached image, else decode the loaded
    /// bytes on the calling thread and cache them. Null while the bytes are not loaded
    /// (spritesheetMissing queues them). Never waits on the async decompressor.
    TSpritesheetPointer decodeNow(const TSpritesheetID& spritesheetId);

Q_SIGNALS:
    void spritesheetUncompressFailed(const tibia::sprites::TSpritesheetID& SpritesheetID);
    void spritesheetMissing(const tibia::sprites::TSpritesheetID& SpritesheetID);

protected Q_SLOTS:
    /// A failed decode is replaced by a magenta spritesheet.
    virtual void onDecompressingSpritesheetFinished(const tibia::sprites::TSpritesheetID& spritesheetId,
                                                    const tibia::sprites::TSpritesheetPointer& spritesheet);

private:
    using TCompressedSpritesheets = QMap<TSpritesheetID, std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>>;

    std::shared_ptr<TSpritesheetDecompressor> m_decompressor;
    TCompressedSpritesheets m_compressedSpritesheets;
    TCompressedSpritesheets m_spritesheetsBeingDecompressed;
    std::shared_ptr<TSpritesheetCache> m_cache;
    mutable QMutex m_mutex;
};

}
}
