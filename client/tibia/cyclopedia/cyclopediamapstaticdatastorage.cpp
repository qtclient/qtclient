#include "cyclopedia/cyclopediamapstaticdatastorage.h"


#include <map.pb.h>

#include <algorithm>
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

bool isZero(const worldmap::TWorldMapCoordinate& coordinate)
{
    return coordinate.x == 0 && coordinate.y == 0 && coordinate.z == 0;
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

const std::vector<TCyclopediaMapStaticDataStorage::TMapFile> NoMapFiles;

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
    m_npcs.clear();
    m_mapFiles.clear();
    m_scales.clear();
    m_areas.clear();
    m_subAreas.clear();

    const worldmap::TWorldMapCoordinate topLeft = coordinateOf(map.top_left_tile_coordinate());
    const worldmap::TWorldMapCoordinate bottomRight = coordinateOf(map.bottom_right_tile_coordinate());
    m_mapExtent = worldmap::TWorldMapCuboid(topLeft, worldmap::TWorldMapExtent(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y, 1));

    for (const auto& npc : map.npcs()) {
        m_npcs.push_back({ coordinateOf(npc.tile_coordinate()), QString::fromStdString(npc.name()),
                           static_cast<quint16>(npc.subarea_id()) });
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
        const auto type = static_cast<EMapFileType>(file.file_type());
        if (type == EMapFileType::Satellite || type == EMapFileType::Minimap) {
            m_scales[type].insert(scale);
            m_mapFiles[type].push_back({ extent, fileName, scale, static_cast<int>(static_cast<float>(width) * scale),
                                         static_cast<int>(static_cast<float>(height) * scale) });
            requestAsset(fileName, false);
        } else if (type == EMapFileType::Subarea) {
            const auto it = m_subAreas.find(static_cast<quint16>(file.area_id()));
            if (it == m_subAreas.end())
                continue;
            it->second.extent = extent;
            it->second.fileName = fileName;
            requestAsset(fileName, true);
        }
    }

    computeAreaExtents();
}

void TCyclopediaMapStaticDataStorage::requestAsset(const QString& fileName, const bool loadErrorReported)
{
    tibia::assets::TAsset asset(tibia::assets::TAsset::MapImage, fileName);
    asset.setLoadErrorReported(loadErrorReported);
    Q_EMIT newAssetDataToLoad(asset, false);
}

void TCyclopediaMapStaticDataStorage::computeAreaExtents()
{
    for (auto& [areaId, area] : m_areas) {
        worldmap::TWorldMapCuboid extent = emptyCuboid();
        for (const quint16 subAreaId : area.subareaIds) {
            const auto it = m_subAreas.find(subAreaId);
            if (it == m_subAreas.end())
                continue;
            if (isZero(area.labelCoordinate) && !(it->second.extent == emptyCuboid()))
                extent = extent == emptyCuboid() ? it->second.extent : unite(extent, it->second.extent);
            it->second.areaId = area.id;
        }
        area.extent = worldmap::TWorldMapCuboid(extent.origin, worldmap::TWorldMapExtent(extent.extent.width, extent.extent.height, 1));
        if (isZero(area.labelCoordinate))
            area.labelCoordinate = area.extent.center();
    }
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

bool TCyclopediaMapStaticDataStorage::isCoordinateInSubArea(const quint16 subAreaId,
                                                            const worldmap::TWorldMapCoordinate& coordinate) const
{
    const auto it = m_subAreas.find(subAreaId);
    if (it == m_subAreas.end() || !reaches(it->second.extent, coordinate))
        return false;
    const_cast<TCyclopediaMapStaticDataStorage*>(this)->resolveSubAreaMasks();
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

void TCyclopediaMapStaticDataStorage::resolveSubAreaMasks()
{
    // CIP resolves each subarea's file through the sprite resolver once the main
    // window exists and keeps the inverted alpha mask; host: on first use.
    for (auto& [subAreaId, subArea] : m_subAreas) {
        if (!subArea.mask.isNull() || subArea.fileName.isEmpty())
            continue;
        const QImage image = mapImage(subArea.fileName);
        if (image.isNull())
            continue;
        QImage mask = image.createAlphaMask(Qt::MonoOnly);
        mask.invertPixels();
        subArea.mask = mask;
    }
}

const std::vector<TCyclopediaMapStaticDataStorage::TMapFile>& TCyclopediaMapStaticDataStorage::mapFiles(const EMapFileType type) const
{
    const auto it = m_mapFiles.find(type);
    return it == m_mapFiles.end() ? NoMapFiles : it->second;
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

}
