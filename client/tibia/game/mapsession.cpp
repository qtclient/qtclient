#include "mapsession.h"

#include <client/position.h>
#include <tibia/worldmap/worldmapstorage.h>
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/currentgamesession.h"
#include "minimap/waypointcosts.h"
#include "worldmap/worldmapfield.h"

#include <cstdlib>
#include <set>

namespace tibia::game {
namespace {

using tibia::worldmap::TWorldMapField;
using tibia::worldmap::TWorldMapStorage;
using TStaticData = tibia::cyclopedia::TCyclopediaMapStaticDataStorage;

/// Where waypointCostsSnapshot takes the live minimap's costs from.
WaypointCostFactory g_waypointCostFactory;

MapPos toMapPos(const tibia::worldmap::TWorldMapCoordinate& coordinate)
{
    return { coordinate.x, coordinate.y, coordinate.z };
}

Position toPosition(const MapPos& p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

MapUseHit fromFieldHit(const TWorldMapField::TFieldItemHit& hit)
{
    if (!hit.ok())
        return {};
    return { true, hit.typeId, hit.stackPos };
}

/// The current session's map (null between sessions).
TWorldMapStorage* storage()
{
    return currentWorldMapStorage();
}

/// The stored field at a map position (null off the map).
TWorldMapField* mapFieldAt(TWorldMapStorage& map, const Position& pos)
{
    return pos.isMapPosition() ? map.fieldAt(pos) : nullptr;
}

} // namespace

bool isWalkable(const MapPos pos, const bool ignoreCreatures)
{
    if (const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos)))
        return tile->isWalkable(ignoreCreatures);
    return false;
}

MapAwareRange awareRange()
{
    const TWorldMapStorage* map = storage();
    if (!map)
        return {};
    const AwareRange range = map->awareRange();
    return { range.left, range.top, range.right, range.bottom };
}

std::vector<MapUseHit> itemHits(const MapPos pos)
{
    const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    std::vector<MapUseHit> out;
    for (const auto& hit : tile->itemHits()) {
        if (const MapUseHit peeled = fromFieldHit(hit); peeled.ok)
            out.push_back(peeled);
    }
    return out;
}

MapUseHit topUseHit(const MapPos pos)
{
    const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    return fromFieldHit(tile->topUseHit());
}

bool isSightClear(const MapPos from, const MapPos to)
{
    const Position fromPos = toPosition(from);
    const Position toPos = toPosition(to);
    if (fromPos == toPos) {
        return true;
    }

    TWorldMapStorage* mapStorage = storage();
    if (!mapStorage)
        return false;
    TWorldMapStorage& map = *mapStorage;
    Position start(fromPos.z > toPos.z ? toPos : fromPos);
    const Position destination(fromPos.z > toPos.z ? fromPos : toPos);

    const int8_t mx = start.x < destination.x ? 1 : start.x == destination.x ? 0 : -1;
    const int8_t my = start.y < destination.y ? 1 : start.y == destination.y ? 0 : -1;

    const int32_t A = destination.y - start.y;
    const int32_t B = start.x - destination.x;
    const int32_t C = -(A * destination.x + B * destination.y);

    while (start.x != destination.x || start.y != destination.y) {
        const int32_t move_hor = std::abs(A * (start.x + mx) + B * (start.y) + C);
        const int32_t move_ver = std::abs(A * (start.x) + B * (start.y + my) + C);
        const int32_t move_cross = std::abs(A * (start.x + mx) + B * (start.y + my) + C);

        if (start.y != destination.y && (start.x == destination.x || move_hor > move_ver || move_hor > move_cross)) {
            start.y += my;
        }

        if (start.x != destination.x && (start.y == destination.y || move_ver > move_hor || move_ver > move_cross)) {
            start.x += mx;
        }

        TWorldMapField* tile = mapFieldAt(map, Position(start.x, start.y, start.z));
        if (tile && !tile->isLookPossible()) {
            return false;
        }
    }

    while (start.z != destination.z) {
        TWorldMapField* tile = mapFieldAt(map, Position(start.x, start.y, start.z));
        if (tile && tile->getThingCount() > 0) {
            return false;
        }
        start.z++;
    }

    return true;
}

std::vector<MapFieldInfo> storedFieldsOnFloor(const int z)
{
    TWorldMapStorage* map = storage();
    if (!map || z < 0 || z > 15)
        return {};
    std::vector<MapFieldInfo> out;
    for (const auto& field : map->fieldList(static_cast<int8_t>(z))) {
        const Position& pos = field->getPosition();
        out.push_back({ { pos.x, pos.y, pos.z }, field->getGroundSpeed(), field->isWalkable(false), field->isPathable() });
    }
    return out;
}

bool isAwareOfPosition(const MapPos pos)
{
    const TWorldMapStorage* map = storage();
    return map && map->isAwareOfPosition(toPosition(pos));
}

void setWaypointCostFactory(WaypointCostFactory factory)
{
    g_waypointCostFactory = std::move(factory);
}

void clearWaypointCostFactory()
{
    g_waypointCostFactory = {};
}

std::shared_ptr<tibia::minimap::TWaypointCosts> waypointCostsSnapshot()
{
    if (g_waypointCostFactory) {
        if (auto costs = g_waypointCostFactory())
            return costs;
    }
    return tibia::minimap::snapshotWaypointCosts(nullptr);
}

uint16_t staticMapSubAreaAt(const MapPos pos)
{
    return TStaticData::instance().subAreaAt(worldmap::TWorldMapCoordinate(pos.x, pos.y, pos.z));
}

bool staticMapArea(const uint16_t id, const bool subArea, StaticMapArea& out)
{
    const TStaticData& data = TStaticData::instance();
    out = {};
    out.id = id;
    out.isSubArea = subArea;
    if (subArea) {
        const TStaticData::TSubArea* found = data.subArea(id);
        if (!found)
            return false;
        out.areaId = found->areaId;
        out.name = found->name.toStdString();
        out.alias = found->alias.toStdString();
        out.label = toMapPos(found->labelCoordinate);
        out.hasMaskFile = !found->fileName.isEmpty();
        if (out.hasMaskFile) {
            out.maskOrigin = toMapPos(found->extent.origin);
            out.maskWidth = found->extent.extent.width;
            out.maskHeight = found->extent.extent.height;
        }
        return true;
    }
    const TStaticData::TArea* found = data.area(id);
    if (!found)
        return false;
    out.name = found->name.toStdString();
    out.alias = found->alias.toStdString();
    out.label = toMapPos(found->labelCoordinate);
    return true;
}

std::vector<uint16_t> staticMapAreaIds(const bool subAreas)
{
    const TStaticData& data = TStaticData::instance();
    std::vector<uint16_t> ids;
    if (subAreas) {
        for (const auto& [id, subArea] : data.subAreas())
            ids.push_back(id);
    } else {
        for (const auto& [id, area] : data.areas())
            ids.push_back(id);
    }
    return ids;
}

std::vector<uint16_t> staticMapSubAreasOfArea(const uint16_t areaId)
{
    const std::set<quint16> ids = TStaticData::instance().subAreaIdsOfArea(areaId);
    return { ids.begin(), ids.end() };
}

} // namespace tibia::game
