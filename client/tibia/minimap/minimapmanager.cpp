#include "minimapmanager.h"

#include <QStandardPaths>

namespace tibia::minimap {

TMinimapManager::TMinimapManager(const QString& defaultTilesDir, const QString& bundledMarkersJson, QObject* parent)
    : cip::tibia::minimap::TMinimapManager(parent)
    , m_defaultTilesDir(defaultTilesDir)
    , m_diskIO(minimapDir())
    , m_imagesCache(this)
    , m_markerController(this)
    , m_markerGameActionHandler(this)
    , m_markerOverlayController(this)
    , m_markerQmlRenderInfo(this)
    , m_protocolMessageHandler(this)
    , m_renderInfoStorage(this)
    , m_tileManager(this)
{
    loadDefaultTilesFromResources();

    connect(&m_storage, &TMinimapTileStorage::startLoadingMinimapTileFromDisk, &m_diskIO, &TMinimapDiskIOAsync::loadTile);
    connect(&m_storage, &TMinimapTileStorage::startSavingMinimapTileToDisk, &m_diskIO, &TMinimapDiskIOAsync::saveTile);
    connect(&m_diskIO, &TMinimapDiskIOAsync::loadingSucceeded, &m_storage, &TMinimapTileStorage::onLoadingSucceeded);
    connect(&m_diskIO, &TMinimapDiskIOAsync::loadingFailed, &m_storage, &TMinimapTileStorage::onLoadingFailed);

    m_worldFeed = std::make_unique<TMinimapWorldFeed>(m_storage);
    m_markers = std::make_unique<TMinimapMarkerStorage>(bundledMarkersJson, this);
}

void TMinimapManager::setProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* handler)
{
    if (m_worldFeed)
        m_worldFeed->setProtocolHandler(handler);
}

TMinimapManager::~TMinimapManager()
{
    m_worldFeed.reset();
    m_markers.reset();
    m_storage.saveAllTilesToDisk();
    m_diskIO.waitForDone();
}

void TMinimapManager::flushToDisk()
{
    if (m_markers)
        m_markers->saveUserMarkers();
    m_storage.saveAllTilesToDisk();
    m_diskIO.waitForDone();
}

void TMinimapManager::loadDefaultTilesFromResources()
{
    if (m_defaultTilesDir.isEmpty())
        return;
    // CipSoft: bulk-copy Resources/minimap → AppData/minimap when empty.
    // Desktop waits so first reads see defaults. Wasm must not waitForDone()
    // (QThreadPool deadlock); pre-Play JS seed usually fills the dir first.
    m_diskIO.importDefaultTiles(m_defaultTilesDir);
#if !defined(__EMSCRIPTEN__)
    m_diskIO.waitForDone();
#endif
}

QString TMinimapManager::minimapDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/minimap");
}

}
