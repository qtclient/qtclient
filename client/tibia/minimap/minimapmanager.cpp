#include "minimapmanager.h"

#include "worldmap/coordinates.h"
#include "worldmap/worldmapstorage.h"

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

    // Field data / isKnown / areaOn only — tile churn comes from TWorldMapStorage.
    m_worldFeed = std::make_unique<TMinimapWorldFeed>(m_storage);
    m_markers = std::make_unique<TMinimapMarkerStorage>(bundledMarkersJson, this);
}

void TMinimapManager::setWorldMapStorage(worldmap::TWorldMapStorage* storage)
{
    if (m_worldMapStorage == storage)
        return;
    if (m_worldMapStorage) {
        disconnect(m_worldMapStorage, nullptr, this, nullptr);
    }
    m_worldMapStorage = storage;
    if (!m_worldMapStorage)
        return;
    connect(m_worldMapStorage, &worldmap::TWorldMapStorage::informationAtWorldMapCoordinateHasChanged, this,
            &TMinimapManager::onWorldMapCoordinateChanged);
    connect(m_worldMapStorage, &worldmap::TWorldMapStorage::storedVolumeHasChanged, this,
            &TMinimapManager::onStoredVolumeChanged);
    // Catch up if the first map description already landed.
    onStoredVolumeChanged(m_worldMapStorage->storedVolume());
}

void TMinimapManager::onWorldMapCoordinateChanged(const worldmap::TWorldMapCoordinate coordinate)
{
    m_storage.onFieldChanged(coordinate.toPosition());
}

void TMinimapManager::onStoredVolumeChanged(const worldmap::TShearedWorldMapCuboid volume)
{
    m_storage.setPlayerZ(volume.topLeft.z);
    m_storage.markUnseen();
}

void TMinimapManager::setProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* /*handler*/)
{
    // Prefer setWorldMapStorage — CIP minimap consumes TWorldMapStorage signals.
}

TMinimapManager::~TMinimapManager()
{
    setWorldMapStorage(nullptr);
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
