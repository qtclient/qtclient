#include "cyclopedia/cyclopediamapstaticdatastorage.h"


#include <map.pb.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <stdexcept>

namespace tibia::cyclopedia {

namespace {

worldmap::TWorldMapCoordinate coordinateOf(const tibia::protobuf::shared::Coordinate& coordinate)
{
    return { static_cast<int>(coordinate.x()), static_cast<int>(coordinate.y()), static_cast<int>(coordinate.z()) };
}

/// The box with no fields at the origin (CIP's default extent).
worldmap::TWorldMapCuboid emptyCuboid()
{
    return worldmap::TWorldMapCuboid(worldmap::TWorldMapCoordinate(0, 0, 0), worldmap::TWorldMapExtent(0, 0, 1));
}

/// The one field `coordinate`.
worldmap::TWorldMapCuboid fieldCuboid(const worldmap::TWorldMapCoordinate& coordinate)
{
    return worldmap::TWorldMapCuboid(coordinate, worldmap::TWorldMapExtent(1, 1, 1));
}

bool isZero(const worldmap::TWorldMapCoordinate& coordinate)
{
    return coordinate.x == 0 && coordinate.y == 0 && coordinate.z == 0;
}

/// Same origin and size (CIP compares those two, not the end).
bool isSameBox(const worldmap::TWorldMapCuboid& first, const worldmap::TWorldMapCuboid& second)
{
    return first.origin == second.origin && first.extent == second.extent;
}

/// The smallest box holding both.
worldmap::TWorldMapCuboid unite(const worldmap::TWorldMapCuboid& first, const worldmap::TWorldMapCuboid& second)
{
    const worldmap::TWorldMapCoordinate origin(std::min(first.origin.x, second.origin.x),
                                               std::min(first.origin.y, second.origin.y),
                                               std::min(first.origin.z, second.origin.z));
    const worldmap::TWorldMapCoordinate end(std::max(first.end.x, second.end.x), std::max(first.end.y, second.end.y),
                                            std::max(first.end.z, second.end.z));
    return worldmap::TWorldMapCuboid(origin, worldmap::TWorldMapExtent(end.x - origin.x, end.y - origin.y, end.z - origin.z));
}

/// The field lies in the box's floor, its far edge included.
bool reaches(const worldmap::TWorldMapCuboid& cuboid, const worldmap::TWorldMapCoordinate& coordinate)
{
    return coordinate.z == cuboid.origin.z && cuboid.origin.x <= coordinate.x && coordinate.x <= cuboid.end.x
        && cuboid.origin.y <= coordinate.y && coordinate.y <= cuboid.end.y;
}

/// The minimap blocks: 256 fields square, at 1/32 of a pixel per field.
constexpr int MinimapTileFields = 256;
constexpr float MinimapTileScale = 0.03125f;
constexpr int MinimapTopFloor = 0;
constexpr int MinimapBottomFloor = 15;

/// `value` cut to a multiple of the block size, toward zero.
int toMinimapTileEdge(const int value)
{
    return (value + (value < 0 ? MinimapTileFields - 1 : 0)) & ~(MinimapTileFields - 1);
}

}

TCyclopediaMapStaticDataStorage::TCyclopediaMapStaticDataStorage(QObject* parent)
    : QObject(parent)
    , m_mapExtent(emptyCuboid())
{
}

void TCyclopediaMapStaticDataStorage::initialize(const QByteArray& mapData)
{
    tibia::protobuf::map::Map map;
    if (!map.ParseFromArray(mapData.constData(), static_cast<int>(mapData.size())))
        throw std::invalid_argument("TCyclopediaMapStaticDataStorage::initialize malformed protobuf data");

    // Host: CIP builds a fresh storage per game client.
    m_npcMarkers.clear();
    m_houseMarkers.clear();
    m_areaMarkers.clear();
    m_subAreaMarkers.clear();
    m_mapTiles.clear();
    m_scales.clear();
    m_areas.clear();
    m_subAreas.clear();

    const worldmap::TWorldMapCoordinate topLeft = coordinateOf(map.top_left_tile_coordinate());
    const worldmap::TWorldMapCoordinate bottomRight = coordinateOf(map.bottom_right_tile_coordinate());
    m_mapExtent = worldmap::TWorldMapCuboid(topLeft, worldmap::TWorldMapExtent(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y, 1));

    for (const auto& npc : map.npcs()) {
        worldmap::TNPCMarkerWorldMapAreaData data;
        data.name = QString::fromStdString(npc.name());
        data.subAreaId = static_cast<quint16>(npc.subarea_id());
        m_npcMarkers.insert(fieldCuboid(coordinateOf(npc.tile_coordinate())), std::move(data));
    }

    for (const auto& area : map.areas()) {
        const auto id = static_cast<quint16>(area.area_id());
        worldmap::TWorldMapCoordinate label;
        if (area.has_label_coordinate())
            label = coordinateOf(area.label_coordinate());
        if (area.area_type() == tibia::protobuf::map::AREA_TYPE_SUBAREA) {
            TSubArea subArea;
            subArea.id = id;
            subArea.name = QString::fromStdString(area.name());
            subArea.alias = QString::fromStdString(area.alias());
            subArea.labelCoordinate = label;
            subArea.extent = emptyCuboid();
            m_subAreas.emplace(id, std::move(subArea));
        } else if (area.area_type() == tibia::protobuf::map::AREA_TYPE_AREA) {
            TArea mainArea;
            mainArea.id = id;
            mainArea.name = QString::fromStdString(area.name());
            mainArea.alias = QString::fromStdString(area.alias());
            for (const auto subAreaId : area.subarea_ids())
                mainArea.subareaIds.insert(static_cast<quint16>(subAreaId));
            mainArea.extent = emptyCuboid();
            mainArea.labelCoordinate = label;
            if (area.has_reject_donations())
                mainArea.rejectDonations = area.reject_donations();
            m_areas.emplace(id, std::move(mainArea));
        }
    }

    for (const auto& file : map.resource_files()) {
        const auto width = static_cast<int>(file.fields_width());
        const auto height = static_cast<int>(file.fields_height());
        const worldmap::TWorldMapCuboid extent(coordinateOf(file.top_left_coordinate()), worldmap::TWorldMapExtent(width, height, 1));
        const auto scale = static_cast<float>(file.scale_factor());
        const QString fileName = QString::fromStdString(file.file_name());
        const auto pixelsWide = static_cast<int>(static_cast<float>(file.fields_width()) * scale);
        const auto pixelsHigh = static_cast<int>(static_cast<float>(file.fields_height()) * scale);
        if (file.file_type() == tibia::protobuf::map::MAP_FILE_TYPE_MINIMAP
            || file.file_type() == tibia::protobuf::map::MAP_FILE_TYPE_SATELLITE) {
            const EMapFileType type = file.file_type() == tibia::protobuf::map::MAP_FILE_TYPE_MINIMAP ? EMapFileType::Minimap
                                                                                                      : EMapFileType::Satellite;
            m_scales[type].insert(scale);
            m_mapTiles[type].insert(extent, worldmap::TMapTileMapAreaData{ fileName, scale, pixelsWide, pixelsHigh });
            requestAsset(fileName, false);
        } else if (file.file_type() == tibia::protobuf::map::MAP_FILE_TYPE_SUBAREA) {
            const auto it = m_subAreas.find(static_cast<quint16>(file.area_id()));
            if (it == m_subAreas.end())
                continue;
            it->second.extent = extent;
            it->second.fileName = fileName;
            it->second.fieldsWidth = width;
            it->second.fieldsHeight = height;
            requestAsset(fileName, true);
        }
    }

    buildAreaData();
}

void TCyclopediaMapStaticDataStorage::requestAsset(const QString& fileName, const bool loadErrorReported)
{
    tibia::assets::TAsset asset(tibia::assets::TAsset::MapImage, fileName);
    asset.setLoadErrorReported(loadErrorReported);
    Q_EMIT newAssetDataToLoad(asset, false);
}

void TCyclopediaMapStaticDataStorage::buildAreaData()
{
    for (auto& [areaId, area] : m_areas) {
        worldmap::TWorldMapCuboid extent = emptyCuboid();
        for (const quint16 subAreaId : area.subareaIds) {
            const auto it = m_subAreas.find(subAreaId);
            if (it == m_subAreas.end())
                continue;
            if (isZero(area.labelCoordinate) && !isSameBox(it->second.extent, emptyCuboid()))
                extent = isSameBox(extent, emptyCuboid()) ? it->second.extent : unite(extent, it->second.extent);
            it->second.areaId = area.id;
        }
        area.extent = worldmap::TWorldMapCuboid(extent.origin, worldmap::TWorldMapExtent(extent.extent.width, extent.extent.height, 1));

        worldmap::TAreaWorldMapAreaData marker;
        marker.id = area.id;
        marker.name = area.name;
        if (isZero(area.labelCoordinate)) {
            marker.labelCoordinate = area.extent.center();
            area.labelCoordinate = marker.labelCoordinate;
        } else {
            marker.labelCoordinate = area.labelCoordinate;
        }
        m_areaMarkers.insert(area.extent, marker);
        m_areaMarkers.insert(fieldCuboid(marker.labelCoordinate), marker);
    }

    for (auto& [subAreaId, subArea] : m_subAreas) {
        worldmap::TSubAreaWorldMapAreaData marker;
        marker.id = subArea.id;
        marker.name = subArea.name;
        marker.extent = subArea.extent;
        marker.fileName = subArea.fileName;
        marker.fieldsWidth = subArea.fieldsWidth;
        marker.fieldsHeight = subArea.fieldsHeight;
        if (isZero(subArea.labelCoordinate)) {
            marker.labelCoordinate = subArea.extent.center();
            subArea.labelCoordinate = marker.labelCoordinate;
        } else {
            marker.labelCoordinate = subArea.labelCoordinate;
        }
        // The subarea's box is a subarea piece; its label is an area piece.
        m_subAreaMarkers.insert(subArea.extent, marker);
        m_areaMarkers.insert(fieldCuboid(marker.labelCoordinate), marker);
    }

    const int firstX = toMinimapTileEdge(m_mapExtent.origin.x);
    const int firstY = toMinimapTileEdge(m_mapExtent.origin.y);
    auto& minimapTiles = m_mapTiles[EMapFileType::MinimapTiles];
    for (int x = firstX; x < m_mapExtent.end.x; x += MinimapTileFields) {
        for (int y = firstY; y < m_mapExtent.end.y; y += MinimapTileFields) {
            for (int z = MinimapTopFloor; z <= MinimapBottomFloor; ++z) {
                const worldmap::TWorldMapCuboid tile(worldmap::TWorldMapCoordinate(x, y, z),
                                                     worldmap::TWorldMapExtent(MinimapTileFields, MinimapTileFields, 1));
                minimapTiles.insert(tile, worldmap::TMinimapTileMapAreaData{ tile.origin, MinimapTileScale, MinimapTileFields,
                                                                            MinimapTileFields });
            }
        }
    }
    m_scales[EMapFileType::MinimapTiles].insert(MinimapTileScale);

    // The NPCs get the area of their subarea.
    worldmap::TWorldMapAreaIndex npcs;
    for (const worldmap::TWorldMapAreaElement& element : m_npcMarkers.elements()) {
        worldmap::TNPCMarkerWorldMapAreaData npc = std::get<worldmap::TNPCMarkerWorldMapAreaData>(element.data);
        const auto it = m_subAreas.find(npc.subAreaId);
        npc.areaId = it == m_subAreas.end() ? 0 : it->second.areaId;
        npcs.insert(element.area, std::move(npc));
    }
    m_npcMarkers = std::move(npcs);
}

std::vector<TCyclopediaMapStaticDataStorage::TNPC> TCyclopediaMapStaticDataStorage::npcs() const
{
    std::vector<TNPC> npcs;
    for (const worldmap::TWorldMapAreaElement& element : m_npcMarkers.elements()) {
        if (const auto* npc = std::get_if<worldmap::TNPCMarkerWorldMapAreaData>(&element.data))
            npcs.push_back({ element.area.origin, npc->name, npc->subAreaId, npc->areaId });
    }
    return npcs;
}

const TCyclopediaMapStaticDataStorage::TArea* TCyclopediaMapStaticDataStorage::area(const quint16 areaId) const
{
    const auto it = m_areas.find(areaId);
    return it == m_areas.end() ? nullptr : &it->second;
}

const TCyclopediaMapStaticDataStorage::TSubArea* TCyclopediaMapStaticDataStorage::subArea(const quint16 subAreaId) const
{
    const auto it = m_subAreas.find(subAreaId);
    return it == m_subAreas.end() ? nullptr : &it->second;
}

QString TCyclopediaMapStaticDataStorage::areaName(const quint16 areaId) const
{
    const TArea* found = area(areaId);
    return found ? found->name : QString();
}

QString TCyclopediaMapStaticDataStorage::subAreaName(const quint16 subAreaId) const
{
    const TSubArea* found = subArea(subAreaId);
    return found ? found->name : QString();
}

QString TCyclopediaMapStaticDataStorage::areaAlias(const quint16 areaId) const
{
    const TArea* found = area(areaId);
    return found ? found->alias : QString();
}

QString TCyclopediaMapStaticDataStorage::subAreaAlias(const quint16 subAreaId) const
{
    const TSubArea* found = subArea(subAreaId);
    return found ? found->alias : QString();
}

bool TCyclopediaMapStaticDataStorage::doesAreaRejectDonations(const quint16 areaId) const
{
    const TArea* found = area(areaId);
    return found && found->rejectDonations;
}

std::set<quint16> TCyclopediaMapStaticDataStorage::subAreaIdsOfArea(const quint16 areaId) const
{
    const TArea* found = area(areaId);
    return found ? found->subareaIds : std::set<quint16>();
}

std::set<quint16> TCyclopediaMapStaticDataStorage::areaIds() const
{
    std::set<quint16> ids;
    for (const auto& [areaId, area] : m_areas)
        ids.insert(areaId);
    return ids;
}

quint16 TCyclopediaMapStaticDataStorage::areaIdOfSubArea(const quint16 subAreaId) const
{
    const TSubArea* found = subArea(subAreaId);
    return found ? found->areaId : 0;
}

bool TCyclopediaMapStaticDataStorage::isCoordinateInSubArea(const quint16 subAreaId,
                                                            const worldmap::TWorldMapCoordinate& coordinate) const
{
    const auto it = m_subAreas.find(subAreaId);
    if (it == m_subAreas.end() || !reaches(it->second.extent, coordinate))
        return false;
    const QImage& mask = it->second.mask;
    const QPoint pixel(coordinate.x - it->second.extent.origin.x, coordinate.y - it->second.extent.origin.y);
    if (!mask.rect().contains(pixel))
        return false;
    return QColor(mask.pixel(pixel)) == QColor(Qt::white);
}

quint16 TCyclopediaMapStaticDataStorage::subAreaAt(const worldmap::TWorldMapCoordinate& coordinate) const
{
    for (const auto& [subAreaId, subArea] : m_subAreas) {
        if (isCoordinateInSubArea(subAreaId, coordinate))
            return subAreaId;
    }
    return 0;
}

void TCyclopediaMapStaticDataStorage::setSubAreaMaskImage(const QString& fileName, const QImage& image)
{
    for (auto& [subAreaId, subArea] : m_subAreas) {
        if (subArea.fileName != fileName)
            continue;
        QImage mask = image.createAlphaMask(Qt::MonoOnly);
        mask.invertPixels();
        subArea.mask = mask;
        return;
    }
}

void TCyclopediaMapStaticDataStorage::addHouseMarkers(const quint32 houseId, const int listing,
                                                      const worldmap::TWorldMapCoordinate& entrance,
                                                      const worldmap::TWorldMapCuboid& houseArea)
{
    for (int floor = houseArea.origin.z; floor < houseArea.end.z; ++floor) {
        const worldmap::TWorldMapCoordinate onFloor =
            worldmap::TWorldMapCoordinate(entrance.x, entrance.y, floor).shiftedTowardsFloor(entrance.z);
        m_houseMarkers.insert(fieldCuboid(onFloor), worldmap::THouseMarkerWorldMapAreaData{ houseId, listing });
    }
}

std::vector<TCyclopediaMapStaticDataStorage::TMapFile> TCyclopediaMapStaticDataStorage::mapFiles(const EMapFileType type) const
{
    std::vector<TMapFile> files;
    const auto it = m_mapTiles.find(type);
    if (it == m_mapTiles.end())
        return files;
    for (const worldmap::TWorldMapAreaElement& element : it->second.elements()) {
        if (const auto* tile = std::get_if<worldmap::TMapTileMapAreaData>(&element.data))
            files.push_back({ element.area, tile->fileName, tile->scale, tile->width, tile->height });
    }
    return files;
}

float TCyclopediaMapStaticDataStorage::levelOfDetail(const float scale, const EMapFileType type) const
{
    const auto it = m_scales.find(type);
    if (it == m_scales.end() || it->second.empty())
        return 1.0f;
    float smallest = *it->second.rbegin();
    for (auto scaleIt = it->second.rbegin(); scaleIt != it->second.rend(); ++scaleIt) {
        if (*scaleIt <= scale)
            return *scaleIt;
        smallest = std::min(smallest, *scaleIt);
    }
    return smallest;
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStaticDataStorage::markersIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    std::vector<worldmap::TWorldMapAreaElement> markers = m_npcMarkers.query(cuboid, 1);
    for (const worldmap::TWorldMapAreaIndex* index : { &m_minimapMarkers, &m_houseMarkers }) {
        std::vector<worldmap::TWorldMapAreaElement> found = index->query(cuboid, 1);
        markers.insert(markers.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
    }
    return markers;
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStaticDataStorage::houseMarkersIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_houseMarkers.query(cuboid, 1);
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStaticDataStorage::subAreasIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_subAreaMarkers.query(cuboid, 1);
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStaticDataStorage::mapTilesIn(const EMapFileType type,
                                                                                       const worldmap::TWorldMapCuboid& cuboid,
                                                                                       const int floors) const
{
    const auto it = m_mapTiles.find(type);
    if (it == m_mapTiles.end())
        return {};
    return it->second.query(cuboid, floors);
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStaticDataStorage::areasIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_areaMarkers.query(cuboid, 1);
}

}
