#include "sprites/spritesheetmanager.h"

#include "sprites/spritesheetdecompressor.h"
#include "sprites/spritesheetdecompressorsynchronous.h"

#include <QMutexLocker>

namespace tibia::sprites {

TSpritesheetManager::TSpritesheetManager(std::shared_ptr<TSpritesheetDecompressor> decompressor,
                                         std::shared_ptr<TSpritesheetCache> cache)
    : m_decompressor(std::move(decompressor))
    , m_cache(std::move(cache))
{
    connect(m_decompressor.get(), &TSpritesheetDecompressor::decompressingSpritesheetFinished, this,
            &TSpritesheetManager::onDecompressingSpritesheetFinished);
}

TSpritesheetManager::~TSpritesheetManager() = default;

void TSpritesheetManager::addCompressedSpritesheet(const TSpritesheetID& spritesheetId,
                                                   const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data)
{
    const QMutexLocker lock(&m_mutex);
    m_compressedSpritesheets.insert(spritesheetId, data);
}

void TSpritesheetManager::addDecompressedSpritesheet(const TSpritesheetID& spritesheetId,
                                                     const TSpritesheetPointer& spritesheet)
{
    const QMutexLocker lock(&m_mutex);
    m_cache->insert(spritesheetId, spritesheet);
    m_compressedSpritesheets.insert(spritesheetId, m_spritesheetsBeingDecompressed.take(spritesheetId));
}

TSpritesheetPointer TSpritesheetManager::getSpritesheet(const TSpritesheetID& spritesheetId)
{
    TSpritesheetPointer spritesheet;
    std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> data;
    {
        const QMutexLocker lock(&m_mutex);
        spritesheet = m_cache->get(spritesheetId);
        if (!spritesheet) {
            data = m_compressedSpritesheets.take(spritesheetId);
            if (!data)
                Q_EMIT spritesheetMissing(spritesheetId);
            else
                m_spritesheetsBeingDecompressed.insert(spritesheetId, data);
        }
    }
    if (!data)
        return spritesheet;
    m_decompressor->decompress(spritesheetId, data);
    if (dynamic_cast<TSpritesheetDecompressorSynchronous*>(m_decompressor.get())) {
        const QMutexLocker lock(&m_mutex);
        spritesheet = m_cache->get(spritesheetId);
    }
    return spritesheet;
}

void TSpritesheetManager::onDecompressingSpritesheetFinished(const TSpritesheetID& spritesheetId,
                                                             const TSpritesheetPointer& spritesheet)
{
    if (spritesheet && !spritesheet->isNull()) {
        addDecompressedSpritesheet(spritesheetId, spritesheet);
        return;
    }
    auto replacement = std::make_shared<QImage>(384, 384, QImage::Format_ARGB32_Premultiplied);
    replacement->fill(Qt::magenta);
    addDecompressedSpritesheet(spritesheetId, replacement);
    Q_EMIT spritesheetUncompressFailed(spritesheetId);
}

bool TSpritesheetManager::isSpritesheetCached(const TSpritesheetID& spritesheetId) const
{
    const QMutexLocker lock(&m_mutex);
    return m_cache->contains(spritesheetId);
}

void TSpritesheetManager::clear()
{
    const QMutexLocker lock(&m_mutex);
    m_compressedSpritesheets.clear();
    m_spritesheetsBeingDecompressed.clear();
    *m_cache = TSpritesheetCache(m_cache->capacity());
}

}
