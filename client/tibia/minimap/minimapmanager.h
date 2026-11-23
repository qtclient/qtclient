#pragma once

#include "minimap/minimapciptypes.h"
#include "minimap/minimapdiskio.h"
#include "minimap/minimapimageschache.h"
#include "minimap/minimapmarkerstorage.h"
#include "minimap/minimaprenderinfostorage.h"
#include "minimap/minimaptilemanager.h"
#include "minimap/minimaptilestorage.h"
#include "minimap/minimapworldfeed.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

namespace tibia {
namespace worldmap {
class TWorldmapProtocolMessageHandler;
}
}

namespace tibia {
namespace minimap {

/// The session's minimap (CIP: TMinimapManager,): the tile
/// storage fed by the world, and its tiles on disk under <app data>/minimap.
/// Lives from entering the world until the session ends; tiles are written
/// when it goes.
class TMinimapManager final : public cip::tibia::minimap::TMinimapManager
{
    Q_OBJECT
public:
    /// `defaultTilesDir`: CIP's bundled minimap, copied in when ours is empty;
    /// its marker file stands in while we have none.
    explicit TMinimapManager(const QString& defaultTilesDir, QObject* parent = nullptr);
    ~TMinimapManager() override;

    TMinimapTileStorage* tileStorage() { return m_storage.get(); }
    /// The tiles the renderer draws; its game actions go through the session's router.
    TMinimapTileManager* tileManager() { return m_tileManager.get(); }
    TMinimapMarkerStorage* markerStorage() { return m_markers.get(); }
    /// The map handler keeps the tile storage current: it sets the floor, marks
    /// unseen fields and records every field it applies. A handler that already
    /// holds a map brings the minimap up to its area at once.
    void setProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* handler);

    static QString minimapDir();
    /// Writes every loaded tile and waits for the disk (CIP vtable slot 0xf8,
    /// before the options page's minimap export).
    void flushToDisk();

private:
    /// Host: copies CipSoft's minimap asset into the user's minimap directory
    /// when that has no tiles yet, and waits so first reads see them.
    void importDefaultTiles();

    QString m_defaultTilesDir;
    std::shared_ptr<TMinimapTileStorage> m_storage;
    QPointer<worldmap::TWorldmapProtocolMessageHandler> m_protocolHandler;
    std::shared_ptr<TMinimapDiskIOAsync> m_diskIO;
    TMinimapImagesChache m_imagesCache;
    TMinimapMarkerGameActionHandler m_markerGameActionHandler;
    TMinimapProtocolMessageHandler m_protocolMessageHandler;
    TMinimapRenderInfoStorage m_renderInfoStorage;
    TMinimapRendererScenegraphDrawCommandProvider m_scenegraphDrawCommands;
    std::shared_ptr<TMinimapTileManager> m_tileManager;
    std::unique_ptr<TMinimapWorldFeed> m_worldFeed;
    std::unique_ptr<TMinimapMarkerStorage> m_markers;
};

}
}
