#include "minimapmanager.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"

#include <QStandardPaths>

namespace tibia::minimap {

TMinimapManager::TMinimapManager(const QString& defaultTilesDir, QObject* parent)
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

    // Field data / isKnown / areaOn only; the map handler drives the updates.
    m_worldFeed = std::make_unique<TMinimapWorldFeed>(m_storage);
    m_markers = std::make_unique<TMinimapMarkerStorage>(this);
    TMinimapMarkerStorage::setActive(m_markers.get());
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setMinimapMarkerStorage(m_markers.get());
    connect(&m_diskIO, &TMinimapDiskIOAsync::loadingMinimapMarkersSucceeded, m_markers.get(),
            &TMinimapMarkerStorage::setMarkersFromMinimapMarkerFile);
    connect(m_markers.get(), &TMinimapMarkerStorage::startSavingMinimapMarkerFileContentToDisk, &m_diskIO,
            &TMinimapDiskIOAsync::saveMinimapMarkerFileContentToDisk);
    m_diskIO.loadMinimapMarkerFileContentFromDiskIfPossible();
}

void TMinimapManager::setProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* handler)
{
    if (m_protocolHandler == handler)
        return;
    if (m_protocolHandler)
        m_protocolHandler->setMinimapTileStorage(nullptr);
    m_protocolHandler = handler;
    if (!handler)
        return;
    handler->setMinimapTileStorage(&m_storage);
    // The minimap can start after the first map arrived: take the floor and
    // unseen fields of the current area as the handler would have.
    if (worldmap::TWorldMapStorage* storage = handler->storage()) {
        const Position center = storage->sessionState().getCentralPosition();
        if (center.isValid()) {
            m_storage.setPlayerZ(center.z);
            m_storage.markUnseen();
        }
    }
}

TMinimapManager::~TMinimapManager()
{
    setProtocolHandler(nullptr);
    m_worldFeed.reset();
    TMinimapMarkerStorage::setActive(nullptr);
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setMinimapMarkerStorage(nullptr);
    if (m_markers)
        m_markers->onDelayedCallback();
    m_markers.reset();
    m_storage.saveAllTilesToDisk();
    m_diskIO.waitForDone();
}

void TMinimapManager::flushToDisk()
{
    if (m_markers)
        m_markers->onDelayedCallback();
    m_storage.saveAllTilesToDisk();
    m_diskIO.waitForDone();
}

void TMinimapManager::loadDefaultTilesFromResources()
{
    if (m_defaultTilesDir.isEmpty())
        return;
    // CipSoft: bulk-copy Resources/minimap → AppData/minimap when empty, and
    // wait so first reads see defaults.
    m_diskIO.importDefaultTiles(m_defaultTilesDir);
    m_diskIO.waitForDone();
}

QString TMinimapManager::minimapDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/minimap");
}

}
