#include "minimapmanager.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "sprites/spriteresolver.h"
#include "sprites/spritesystem.h"

#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"

#include <QStandardPaths>

namespace tibia::minimap {

TMinimapManager::TMinimapManager(const QString& defaultTilesDir, QObject* parent)
    : cip::tibia::minimap::TMinimapManager(parent)
    , m_defaultTilesDir(defaultTilesDir)
    , m_storage(std::make_shared<TMinimapTileStorage>())
    , m_diskIO(std::make_shared<TMinimapDiskIOAsync>(minimapDir()))
    , m_markerGameActionHandler(this)
    , m_protocolMessageHandler(this)
{
    importDefaultTiles();

    TMinimapTileStorage* storage = m_storage.get();
    TMinimapDiskIOAsync* diskIO = m_diskIO.get();
    connect(storage, &TMinimapTileStorage::startLoadingMinimapTileFromDisk, diskIO, &TMinimapDiskIOAsync::loadTile);
    connect(storage, &TMinimapTileStorage::startSavingMinimapTileToDisk, diskIO, &TMinimapDiskIOAsync::saveTile);
    connect(diskIO, &TMinimapDiskIOAsync::loadingSucceeded, storage, &TMinimapTileStorage::onLoadingSucceeded);
    connect(diskIO, &TMinimapDiskIOAsync::loadingFailed, storage, &TMinimapTileStorage::onLoadingFailed);
    m_tileManager = std::make_shared<TMinimapTileManager>(m_diskIO, m_storage);
    sprites::TSpriteSystem::instance().resolver().setMinimapTileManager(m_tileManager);

    // Field data / isKnown / areaOn only; the map handler drives the updates.
    m_worldFeed = std::make_unique<TMinimapWorldFeed>(*storage);
    m_markers = std::make_unique<TMinimapMarkerStorage>(this);
    TMinimapMarkerStorage::setActive(m_markers.get());
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setMinimapMarkerStorage(m_markers.get());
    connect(diskIO, &TMinimapDiskIOAsync::loadingMinimapMarkersSucceeded, m_markers.get(),
            &TMinimapMarkerStorage::setMarkersFromMinimapMarkerFile);
    connect(m_markers.get(), &TMinimapMarkerStorage::startSavingMinimapMarkerFileContentToDisk, diskIO,
            &TMinimapDiskIOAsync::saveMinimapMarkerFileContentToDisk);
    m_diskIO->loadMinimapMarkerFileContentFromDiskIfPossible();
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
    handler->setMinimapTileStorage(m_storage.get());
    // The minimap can start after the first map arrived: take the floor and
    // unseen fields of the current area as the handler would have.
    if (worldmap::TWorldMapStorage* storage = handler->storage()) {
        const Position center = storage->sessionState().getCentralPosition();
        if (center.isValid()) {
            m_storage->setPlayerZ(center.z);
            m_storage->markUnseen();
        }
    }
}

TMinimapManager::~TMinimapManager()
{
    setProtocolHandler(nullptr);
    sprites::TSpriteSystem::instance().resolver().setMinimapTileManager(nullptr);
    m_worldFeed.reset();
    TMinimapMarkerStorage::setActive(nullptr);
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setMinimapMarkerStorage(nullptr);
    if (m_markers)
        m_markers->onDelayedCallback();
    m_markers.reset();
    m_storage->saveAllTilesToDisk();
    m_diskIO->waitForDone();
}

void TMinimapManager::flushToDisk()
{
    if (m_markers)
        m_markers->onDelayedCallback();
    m_storage->saveAllTilesToDisk();
    m_diskIO->waitForDone();
}

void TMinimapManager::importDefaultTiles()
{
    if (m_defaultTilesDir.isEmpty())
        return;
    m_diskIO->importDefaultTiles(m_defaultTilesDir);
    m_diskIO->waitForDone();
}

QString TMinimapManager::minimapDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/minimap");
}

}
