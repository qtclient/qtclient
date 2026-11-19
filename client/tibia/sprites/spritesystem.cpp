#include "sprites/spritesystem.h"

#include "assets/assetasyncloader.h"
#include "assets/assetfilesystemhelper.h"
#include "assets/contentcatalog.h"
#include "sprites/sprite.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesheetdecompressorthreadpool.h"
#include "sprites/spritesheetmanager.h"

#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#include <sys/sysctl.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_UNIX) && !defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

namespace tibia::sprites {

namespace {

quint64 physicalMemory()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    quint64 bytes = 0;
    size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0)
        return bytes;
    return 0;
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#elif defined(Q_OS_UNIX) && !defined(__EMSCRIPTEN__)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && pageSize > 0 ? quint64(pages) * quint64(pageSize) : 0;
#else
    return 0;
#endif
}

/// The metadata manager remembers its last lookup; the renderer asks from its thread.
QMutex& metadataMutex()
{
    static QMutex mutex;
    return mutex;
}

}

uint32_t TSpriteSystem::defaultCacheCapacity()
{
    const quint64 bytes = std::min<quint64>(physicalMemory() / 2, 0x20000000);
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
    const uint32_t capacity = defaultCacheCapacity();
    qDebug() << "Using spritesheet cache size of" << capacity;
    m_spritesheets = std::make_shared<TSpritesheetManager>(m_decompressor, std::make_shared<TSpritesheetCache>(capacity));
    m_resolver = std::make_unique<TSpriteResolver>(m_spritesheets);
}

void TSpriteSystem::clear()
{
    m_assetLoader.reset();
    {
        const QMutexLocker lock(&metadataMutex());
        m_metadata.reset();
    }
    m_spritesheets->clear();
}

void TSpriteSystem::start(const tibia::assets::TContentCatalog& catalog, const QString& assetsDirectory)
{
    clear();
    const QDir directory(assetsDirectory);
    {
        const QMutexLocker lock(&metadataMutex());
        m_metadata = catalog.getSpritesheetsMetadataManager();
    }
    m_assetLoader = std::make_unique<tibia::assets::TAssetAsyncLoader>(
        std::make_shared<tibia::assets::TAssetFileSystemHelper>(directory, directory));
    QObject::connect(m_assetLoader.get(), &tibia::assets::TAssetAsyncLoader::assetLoaded, m_spritesheets.get(),
                     [this](const tibia::assets::TAsset& asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data) {
                         // CIP's asset handler gives sprite and map image files to the spritesheets.
                         if (asset.type() == tibia::assets::TAsset::Sprite || asset.type() == tibia::assets::TAsset::MapImage)
                             m_spritesheets->addCompressedSpritesheet(asset.file(), data);
                     });
    QObject::connect(m_assetLoader.get(), &tibia::assets::TAssetAsyncLoader::assetLoadError, m_spritesheets.get(),
                     [](const tibia::assets::TAsset& asset, const std::string& error) {
                         qWarning() << "Failed to load asset" << asset.file() << QString::fromStdString(error);
                     });
    QObject::connect(m_spritesheets.get(), &TSpritesheetManager::spritesheetMissing, m_assetLoader.get(),
                     &tibia::assets::TAssetAsyncLoader::priorityLoadOfSpritesheet, Qt::QueuedConnection);
    for (const auto& asset : catalog.assets()) {
        if (asset.type() == tibia::assets::TAsset::Sprite)
            m_assetLoader->load(asset, false);
    }
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

bool TSpriteSystem::isSpriteReady(const uint32_t spriteId)
{
    const TSpriteFromSpritesheetInfo info = spriteInfo(spriteId);
    return info.spritesheetId.isEmpty() || m_spritesheets->isSpritesheetCached(info.spritesheetId);
}

}
