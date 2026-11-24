#include "sprites/spritesystem.h"

#include "assets/assetasyncloader.h"
#include "sprites/sprite.h"
#include "sprites/spriteinspritesheet.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesheetdecompressorthreadpool.h"
#include "sprites/spritesheetmanager.h"

#include <QMutex>
#include <QMutexLocker>

#include "platform/platform.h"

namespace tibia::sprites {

namespace {

/// The metadata manager remembers its last lookup; the renderer asks from its thread.
QMutex& metadataMutex()
{
    static QMutex mutex;
    return mutex;
}

/// The game client hands its spritesheet manager in on the GUI thread; the
/// renderer's threads read it.
QMutex& spritesheetsMutex()
{
    static QMutex mutex;
    return mutex;
}

}

uint32_t TSpriteSystem::defaultCacheCapacity()
{
    const quint64 bytes = std::min<quint64>(tibia::platform::physicalMemory() / 2, 0x20000000);
    const uint32_t spritesheets = static_cast<uint32_t>(bytes / (384 * 384 * 4));
    return std::max<uint32_t>(spritesheets, 350);
}

TSpriteSystem& TSpriteSystem::instance()
{
    static TSpriteSystem system;
    return system;
}

TSpriteSystem::TSpriteSystem()
{
    m_decompressor = std::make_shared<TSpritesheetDecompressorThreadpool>();
    m_ownSpritesheets = std::make_shared<TSpritesheetManager>(m_decompressor,
                                                              std::make_shared<TSpritesheetCache>(defaultCacheCapacity()));
    m_spritesheets = m_ownSpritesheets;
    m_ownResolver = std::make_shared<TSpriteResolver>(m_ownSpritesheets);
    m_resolver = m_ownResolver;
}

void TSpriteSystem::clear()
{
    m_assetLoader.reset();
    {
        const QMutexLocker lock(&metadataMutex());
        m_metadata.reset();
    }
    sharedSpritesheets()->clear();
}

void TSpriteSystem::setSpritesheetManager(std::shared_ptr<TSpritesheetManager> spritesheets)
{
    const QMutexLocker lock(&spritesheetsMutex());
    m_spritesheets = spritesheets ? std::move(spritesheets) : m_ownSpritesheets;
}

std::shared_ptr<TSpritesheetManager> TSpriteSystem::sharedSpritesheets() const
{
    const QMutexLocker lock(&spritesheetsMutex());
    return m_spritesheets;
}

void TSpriteSystem::setMetadata(std::shared_ptr<TSpritesheetsMetadataManager> metadata)
{
    const QMutexLocker lock(&metadataMutex());
    m_metadata = std::move(metadata);
}

void TSpriteSystem::setResolver(std::shared_ptr<TSpriteResolver> resolver)
{
    m_resolver = resolver ? std::move(resolver) : m_ownResolver;
}

void TSpriteSystem::setAssetLoader(std::shared_ptr<tibia::assets::TAssetAsyncLoader> assetLoader)
{
    m_assetLoader = std::move(assetLoader);
}

void TSpriteSystem::withMetadataLocked(const std::function<void()>& use)
{
    const QMutexLocker lock(&metadataMutex());
    use();
}

TSpriteFromSpritesheetInfo TSpriteSystem::spriteInfo(const uint32_t spriteId, const bool opaque)
{
    TSpritesheetMetadata metadata;
    {
        const QMutexLocker lock(&metadataMutex());
        if (!m_metadata)
            return {};
        metadata = m_metadata->getMetadataForSprite(spriteId);
    }
    if (!metadata.isValid())
        return {};
    return TSpriteFromSpritesheetInfo(spriteId, metadata.spritesheetId,
                                      TSpriteFromSpritesheetInfo::cellInSpritesheet(metadata, spriteId), opaque);
}

QSize TSpriteSystem::spriteSize(const uint32_t spriteId)
{
    const QMutexLocker lock(&metadataMutex());
    if (!m_metadata)
        return {};
    const TSpritesheetMetadata metadata = m_metadata->getMetadataForSprite(spriteId);
    return metadata.isValid() ? spriteSizeOfLayout(metadata.getLayout()) : QSize();
}

QImage TSpriteSystem::spriteImage(const uint32_t spriteId, bool& loading)
{
    loading = false;
    if (spriteId == 0)
        return {};
    const TSpriteFromSpritesheetInfo info = spriteInfo(spriteId);
    if (info.spritesheetId.isEmpty())
        return {};
    const std::shared_ptr<TSprite> sprite = m_resolver->resolveSpritesheetSprite(info);
    if (!sprite) {
        loading = true;
        return {};
    }
    return sprite->getImage().convertToFormat(QImage::Format_RGBA8888);
}

QImage TSpriteSystem::spriteImageDecoded(const uint32_t spriteId, bool& loading)
{
    loading = false;
    if (spriteId == 0)
        return {};
    const TSpriteFromSpritesheetInfo info = spriteInfo(spriteId);
    if (info.spritesheetId.isEmpty())
        return {};
    const TSpritesheetPointer spritesheet = sharedSpritesheets()->decodeNow(info.spritesheetId);
    if (!spritesheet) {
        loading = true;
        return {};
    }
    // Same cell/area as TSpriteResolver::resolveSpritesheetSprite.
    const QRect area(QPoint(0, 0), QPoint(info.cell.right() - info.cell.left(), info.cell.bottom() - info.cell.top()));
    return TSpriteInSpritesheet(0.0, spritesheet, info.cell, area, info.opaque).getImage().convertToFormat(QImage::Format_RGBA8888);
}

bool TSpriteSystem::isSpriteReady(const uint32_t spriteId)
{
    const TSpriteFromSpritesheetInfo info = spriteInfo(spriteId);
    return info.spritesheetId.isEmpty() || sharedSpritesheets()->isSpritesheetCached(info.spritesheetId);
}

}
