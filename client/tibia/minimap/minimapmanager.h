#pragma once

#include "minimap/minimapciptypes.h"
#include "minimap/minimapdiskio.h"
#include "minimap/minimapmarkerstorage.h"
#include "minimap/minimaptilestorage.h"
#include "minimap/minimapworldfeed.h"

#include <cip_gen.h>

#include <memory>

namespace tibia::worldmap {
class TWorldmapProtocolMessageHandler;
}

namespace tibia::minimap {

/// The session's minimap (CIP: TMinimapManager, 0x100ebe8c0): the tile
/// storage fed by the world, and its tiles on disk under <app data>/minimap.
/// Lives from entering the world until the session ends; tiles are written
/// when it goes.
class TMinimapManager final : public cip::tibia::minimap::TMinimapManager
{
    Q_OBJECT
public:
    /// `defaultTilesDir`: CIP's bundled minimap, copied in when ours is empty.
    /// `bundledMarkersJson`: optional tibiamaps `minimap-markers.json`.
    explicit TMinimapManager(const QString& defaultTilesDir, const QString& bundledMarkersJson = {},
                             QObject* parent = nullptr);
    ~TMinimapManager() override;

    TMinimapTileStorage* tileStorage() { return &m_storage; }
    TMinimapMarkerStorage* markerStorage() { return m_markers.get(); }
    void setProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* handler);

    static QString minimapDir();
    /// Writes every loaded tile and waits for the disk (CIP vtable slot 0xf8,
    /// before the options page's minimap export).
    void flushToDisk();
    /// CIP LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES: copy bundled tiles when empty.
    void loadDefaultTilesFromResources();

private:
    QString m_defaultTilesDir;
    TMinimapTileStorage m_storage;
    TMinimapDiskIOAsync m_diskIO;
    TMinimapDiskIOMemoryOnly m_diskIOMemoryOnly;
    TMinimapDiskIOSynchronous m_diskIOSynchronous;
    TMinimapImagesChache m_imagesCache;
    TMinimapMarkerController m_markerController;
    TMinimapMarkerGameActionHandler m_markerGameActionHandler;
    TMinimapMarkerOverlayController m_markerOverlayController;
    TMinimapMarkerQmlRenderInfo m_markerQmlRenderInfo;
    TMinimapMarkerRenderProvider m_markerRenderProvider;
    TMinimapProtocolMessageHandler m_protocolMessageHandler;
    TMinimapRenderInfoStorage m_renderInfoStorage;
    TMinimapRendererScenegraphDrawCommandProvider m_scenegraphDrawCommands;
    TMinimapTileManager m_tileManager;
    std::unique_ptr<TMinimapWorldFeed> m_worldFeed;
    std::unique_ptr<TMinimapMarkerStorage> m_markers;
};

}
