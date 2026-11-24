#pragma once

#include "worldmap/coordinates.h"
#include "worldmap/worldmapareadata.h"

#include <cip_gen.h>

#include <QList>

#include <array>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
class TCyclopediaMapStorage;
}
namespace renderer {
class TWorldMapCamera;
}
namespace sprites {
class TResourceSpritesManager;
}
namespace minimap {

/// What the Cyclopedia map's renderer draws (CIP: TMinimapRenderInfoStorage):
/// the map tile, subarea and marker pieces of map data the camera shows, taken
/// from the static map data and the Cyclopedia map storage and filtered by the
/// renderer's settings. The subarea and marker lists are cached until a
/// source changes; each change of a source marks them outdated and emits
/// renderInfosChanged.
class TMinimapRenderInfoStorage final : public cip::tibia::minimap::TMinimapRenderInfoStorage
{
    Q_OBJECT
public:
    /// The kinds of map tiles drawn (the renderer's EMinimapTileType):
    /// satellite images, minimap images and the client's own minimap.
    enum class ETileType { Satellite = 0, Minimap = 1, LocalMinimap = 2, AfterLast = 3 };

    TMinimapRenderInfoStorage();
    ~TMinimapRenderInfoStorage() override;

    /// The four outdated flags, in CipSoft's order: markers, subareas, and two
    /// more a camera change also sets.
    const std::array<bool, 4>& outdatedFlags() const { return m_outdated; }

    renderer::TWorldMapCamera* camera() const { return m_camera; }
    /// Follows the camera's viewport changes.
    void setCamera(renderer::TWorldMapCamera* camera);
    cyclopedia::TCyclopediaMapStaticDataStorage* staticDataStorage() const { return m_staticDataStorage; }
    void setStaticDataStorage(cyclopedia::TCyclopediaMapStaticDataStorage* storage);
    cyclopedia::TCyclopediaMapStorage* mapStorage() const { return m_mapStorage; }
    void setMapStorage(cyclopedia::TCyclopediaMapStorage* storage);
    /// The resource images the markers are drawn with; no markers are drawn without.
    sprites::TResourceSpritesManager* resourceSpritesManager() const { return m_resourceSpritesManager; }
    void setResourceSpritesManager(sprites::TResourceSpritesManager* manager) { m_resourceSpritesManager = manager; }

    /// NPC markers: only those of main areas with a status are shown.
    bool showNPCMarkers() const { return m_showNPCMarkers; }
    void setShowNPCMarkers(bool show) { m_showNPCMarkers = show; }
    bool showPassages() const { return m_showPassages; }
    void setShowPassages(bool show) { m_showPassages = show; }
    bool showHouses() const { return m_showHouses; }
    void setShowHouses(bool show) { m_showHouses = show; }
    /// Viewpoints of the subarea being discovered.
    bool showViewpoints() const { return m_showViewpoints; }
    void setShowViewpoints(bool show);
    /// One flag per minimap marker icon: the icons whose flag is set are shown.
    QList<bool> minimapMarkersFilter() const { return m_minimapMarkersFilter; }
    void setMinimapMarkersFilter(const QList<bool>& filter);
    /// The marker list is outdated: rebuilt on the next query.
    void markMarkersOutdated();

    ETileType tileType() const { return m_tileType; }
    /// Marks the marker list outdated when the type changes.
    void setTileType(ETileType tileType);

    /// The subareas whose overlays are drawn (the selection).
    const std::set<quint16>& subAreasToShow() const { return m_subAreasToShow; }
    void setSubAreasToShow(const std::set<quint16>& subAreas);
    /// The passage drawn selected.
    void setSelectedPassage(const worldmap::TWorldMapCoordinate& coordinate);

    /// The map tile pieces of the tile type on `floors` floors from the camera's
    /// floor in its visible area, at the level of detail of its scale factor.
    std::vector<worldmap::TWorldMapAreaElement> mapTileAreaData(int floors) const;
    /// The shown subareas' pieces on the ground floor under the visible area;
    /// none while the camera is below ground.
    std::vector<worldmap::TWorldMapAreaElement> subAreaAreaData();
    /// The marker pieces in the visible area that the settings show: NPCs,
    /// minimap flags and house entrances of the static data, then passages
    /// (the selected one marked), viewpoints, raids and the player.
    std::vector<worldmap::TWorldMapAreaElement> markerAreaData();

public Q_SLOTS:
    void onCameraViewportChanged();
    void onPassagesChanged();
    void onViewpointsChanged();
    void onActiveRaidsChanged();
    void onMinimapMarkersChanged();
    void onPlayerPositionChanged();
    void onClickedAreaChanged();

private:
    /// A source other than the camera changed: flags one and three.
    void markContentOutdated();

    renderer::TWorldMapCamera* m_camera = nullptr;
    cyclopedia::TCyclopediaMapStaticDataStorage* m_staticDataStorage = nullptr;
    cyclopedia::TCyclopediaMapStorage* m_mapStorage = nullptr;
    sprites::TResourceSpritesManager* m_resourceSpritesManager = nullptr;
    std::vector<worldmap::TWorldMapAreaElement> m_markerAreaData;
    std::vector<worldmap::TWorldMapAreaElement> m_subAreaAreaData;
    std::array<bool, 4> m_outdated{};
    /// The minimap marker icons shown (from the filter).
    std::set<quint8> m_shownMinimapMarkerIcons;
    bool m_showNPCMarkers = false;
    bool m_showPassages = false;
    bool m_showHouses = false;
    bool m_showViewpoints = false;
    QList<bool> m_minimapMarkersFilter;
    ETileType m_tileType = ETileType::Satellite;
    std::set<quint16> m_subAreasToShow;
    worldmap::TWorldMapCoordinate m_selectedPassage;
    QMetaObject::Connection m_cameraConnection;
    QMetaObject::Connection m_staticDataConnection;
    QMetaObject::Connection m_mapStorageConnection;
};

}
}
