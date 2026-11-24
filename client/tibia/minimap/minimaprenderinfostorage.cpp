#include "minimap/minimaprenderinfostorage.h"

#include "cyclopedia/cyclopediamapdata.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "renderer/worldmapcamera.h"

#include <iterator>
#include <variant>

namespace tibia::minimap {

namespace {

constexpr int GroundFloor = 7;

void append(std::vector<worldmap::TWorldMapAreaElement>& to, std::vector<worldmap::TWorldMapAreaElement> from)
{
    to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
}

/// Keeps the map tiles drawn at the level of detail (CIP: TFilterForScaleFactorVisitor).
struct TFilterForScaleFactorVisitor
{
    float scale;

    bool operator()(const worldmap::TMapTileMapAreaData& data) const { return scale == data.scale; }
    bool operator()(const worldmap::TMinimapTileMapAreaData& data) const { return scale == data.scale; }
    template<typename T>
    bool operator()(const T&) const
    {
        return false;
    }
};

/// Keeps the subareas of the selection (CIP: TFilterForSubAreasToShowVisitor).
struct TFilterForSubAreasToShowVisitor
{
    const std::set<quint16>& subAreas;

    bool operator()(const worldmap::TSubAreaWorldMapAreaData& data) const { return subAreas.contains(data.id); }
    template<typename T>
    bool operator()(const T&) const
    {
        return false;
    }
};

/// Keeps the markers the settings show (CIP: TFilterForMarkerTypeVisitor).
struct TFilterForMarkerTypeVisitor
{
    const std::set<quint8>& minimapMarkerIcons;
    const std::set<quint16>& areas;
    bool showPassages;
    bool showViewpoints;
    bool showActiveRaids;
    bool showHouses;

    bool operator()(const worldmap::TNPCMarkerWorldMapAreaData& data) const { return areas.contains(data.areaId); }
    bool operator()(const worldmap::TMinimapMarkerWorldMapAreaData& data) const
    {
        return minimapMarkerIcons.contains(data.icon);
    }
    bool operator()(const worldmap::TPassageMapAreaData&) const { return showPassages; }
    bool operator()(const worldmap::TViewpointMapAreaData& data) const { return data.discovered && showViewpoints; }
    bool operator()(const worldmap::TActiveRaidMapAreaData&) const { return showActiveRaids; }
    bool operator()(const worldmap::TPlayerPositionMapAreaData&) const { return true; }
    bool operator()(const worldmap::THouseMarkerWorldMapAreaData&) const { return showHouses; }
    template<typename T>
    bool operator()(const T&) const
    {
        return false;
    }
};

}

TMinimapRenderInfoStorage::TMinimapRenderInfoStorage()
    : cip::tibia::minimap::TMinimapRenderInfoStorage(nullptr)
{
}

TMinimapRenderInfoStorage::~TMinimapRenderInfoStorage() = default;

void TMinimapRenderInfoStorage::setCamera(renderer::TWorldMapCamera* camera)
{
    if (m_camera == camera)
        return;
    disconnect(m_cameraConnection);
    m_camera = camera;
    if (camera) {
        m_cameraConnection = connect(camera, &renderer::TWorldMapCamera::cameraViewportChanged, this,
                                     &TMinimapRenderInfoStorage::onCameraViewportChanged);
    }
    onCameraViewportChanged();
}

void TMinimapRenderInfoStorage::setStaticDataStorage(cyclopedia::TCyclopediaMapStaticDataStorage* storage)
{
    if (m_staticDataStorage == storage)
        return;
    disconnect(m_staticDataConnection);
    m_staticDataStorage = storage;
    if (storage) {
        m_staticDataConnection = connect(storage, &cyclopedia::TCyclopediaMapStaticDataStorage::minimapMarkersChanged,
                                         this, &TMinimapRenderInfoStorage::onMinimapMarkersChanged);
    }
    markContentOutdated();
}

void TMinimapRenderInfoStorage::setMapStorage(cyclopedia::TCyclopediaMapStorage* storage)
{
    if (m_mapStorage == storage)
        return;
    disconnect(m_mapStorageConnection);
    m_mapStorage = storage;
    if (storage) {
        // Host: the storage announces every change of its passages, viewpoints,
        // raids and player position with one signal.
        m_mapStorageConnection = connect(storage, &cyclopedia::TCyclopediaMapStorage::changed, this,
                                         &TMinimapRenderInfoStorage::onPassagesChanged);
    }
    markContentOutdated();
}

void TMinimapRenderInfoStorage::setShowViewpoints(const bool show)
{
    m_showViewpoints = show;
}

void TMinimapRenderInfoStorage::setMinimapMarkersFilter(const QList<bool>& filter)
{
    m_minimapMarkersFilter = filter;
    m_shownMinimapMarkerIcons.clear();
    for (qsizetype icon = 0; icon < filter.size(); ++icon) {
        if (filter.at(icon))
            m_shownMinimapMarkerIcons.insert(static_cast<quint8>(icon));
    }
}

void TMinimapRenderInfoStorage::markMarkersOutdated()
{
    m_outdated[0] = true;
    m_outdated[2] = true;
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::setTileType(const ETileType tileType)
{
    if (m_tileType == tileType)
        return;
    m_tileType = tileType;
    markMarkersOutdated();
}

void TMinimapRenderInfoStorage::setSubAreasToShow(const std::set<quint16>& subAreas)
{
    if (m_subAreasToShow == subAreas)
        return;
    m_subAreasToShow = subAreas;
    m_outdated[1] = true;
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::setSelectedPassage(const worldmap::TWorldMapCoordinate& coordinate)
{
    if (m_selectedPassage == coordinate)
        return;
    m_selectedPassage = coordinate;
    markMarkersOutdated();
}

std::vector<worldmap::TWorldMapAreaElement> TMinimapRenderInfoStorage::mapTileAreaData(const int floors) const
{
    std::vector<worldmap::TWorldMapAreaElement> tiles;
    if (!m_staticDataStorage || !m_camera)
        return tiles;
    const worldmap::TWorldMapCuboid visibleArea = m_camera->visibleArea();
    const auto type = static_cast<cyclopedia::TCyclopediaMapStaticDataStorage::EMapFileType>(m_tileType);
    const float levelOfDetail = m_staticDataStorage->levelOfDetail(m_camera->scaleFactor(), type);
    for (worldmap::TWorldMapAreaElement& element : m_staticDataStorage->mapTilesIn(type, visibleArea, floors)) {
        if (std::visit(TFilterForScaleFactorVisitor{ levelOfDetail }, element.data))
            tiles.push_back(std::move(element));
    }
    return tiles;
}

std::vector<worldmap::TWorldMapAreaElement> TMinimapRenderInfoStorage::subAreaAreaData()
{
    if (m_outdated[1]) {
        m_outdated[1] = false;
        m_subAreaAreaData.clear();
        if (m_camera && m_staticDataStorage) {
            const worldmap::TWorldMapCuboid visibleArea = m_camera->visibleArea();
            if (visibleArea.origin.z <= GroundFloor) {
                const TFilterForSubAreasToShowVisitor filter{ m_subAreasToShow };
                for (worldmap::TWorldMapAreaElement& element :
                     m_staticDataStorage->subAreasIn(visibleArea.floorArea(GroundFloor))) {
                    if (std::visit(filter, element.data))
                        m_subAreaAreaData.push_back(std::move(element));
                }
            }
        }
    }
    return m_subAreaAreaData;
}

std::vector<worldmap::TWorldMapAreaElement> TMinimapRenderInfoStorage::markerAreaData()
{
    if (m_outdated[0]) {
        m_outdated[0] = false;
        m_markerAreaData.clear();
        if (m_camera) {
            const worldmap::TWorldMapCuboid visibleArea = m_camera->visibleArea();
            std::vector<worldmap::TWorldMapAreaElement> markers;
            if (m_staticDataStorage)
                append(markers, m_staticDataStorage->markersIn(visibleArea));
            std::set<quint16> areas;
            if (m_mapStorage) {
                for (worldmap::TWorldMapAreaElement& element : m_mapStorage->passagesIn(visibleArea)) {
                    auto& passage = std::get<worldmap::TPassageMapAreaData>(element.data);
                    passage.selected = passage.coordinate == m_selectedPassage;
                    markers.push_back(std::move(element));
                }
                append(markers, m_mapStorage->viewpointsIn(visibleArea));
                append(markers, m_mapStorage->activeRaidsIn(visibleArea));
                markers.push_back(m_mapStorage->playerPositionElement());
                if (m_showNPCMarkers)
                    areas = m_mapStorage->mainAreasWithStatus();
            }
            const TFilterForMarkerTypeVisitor filter{ m_shownMinimapMarkerIcons, areas, m_showPassages, m_showViewpoints,
                                                      true, m_showHouses };
            for (worldmap::TWorldMapAreaElement& element : markers) {
                if (std::visit(filter, element.data))
                    m_markerAreaData.push_back(std::move(element));
            }
        }
    }
    return m_markerAreaData;
}

void TMinimapRenderInfoStorage::onCameraViewportChanged()
{
    m_outdated = { true, true, true, true };
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::markContentOutdated()
{
    m_outdated[0] = true;
    m_outdated[2] = true;
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::onPassagesChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onViewpointsChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onActiveRaidsChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onMinimapMarkersChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onPlayerPositionChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onClickedAreaChanged()
{
    markContentOutdated();
}

}
