#include "mapsession.h"

#include <client/position.h>
#include <tibia/world/map.h>
#include "worldmap/worldmapfield.h"

namespace tibia::game {
namespace {

Position toPosition(const MapPos& p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

MapUseHit fromFieldHit(const tibia::worldmap::TWorldMapField::TFieldItemHit& hit)
{
    if (!hit.ok())
        return {};
    return { true, hit.typeId, hit.stackPos };
}

} // namespace

bool canTravelPath(const MapPos from, const MapPos to, const int maxComplexity)
{
    return g_map.canTravelPath(toPosition(from), toPosition(to), maxComplexity);
}

bool isWalkable(const MapPos pos, const bool ignoreCreatures)
{
    if (const tibia::worldmap::TWorldMapField* tile = g_map.getTile(toPosition(pos)))
        return tile->isWalkable(ignoreCreatures);
    return false;
}

MapAwareRange awareRange()
{
    const AwareRange range = g_map.getAwareRange();
    return { range.left, range.top, range.right, range.bottom };
}

std::vector<MapUseHit> itemHits(const MapPos pos)
{
    const tibia::worldmap::TWorldMapField* tile = g_map.getTile(toPosition(pos));
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
    const tibia::worldmap::TWorldMapField* tile = g_map.getTile(toPosition(pos));
    if (!tile)
        return {};
    return fromFieldHit(tile->topUseHit());
}

std::vector<MapItemHit> findItemsInRange(const MapPos center, const int rangeX, const int rangeY,
                                         const std::vector<uint16_t>& typeIds)
{
    std::vector<MapItemHit> out;
    for (const Map::TItemHit& hit : g_map.findItemsInRange(toPosition(center), rangeX, rangeY, typeIds)) {
        out.push_back({
            { hit.pos.x, hit.pos.y, hit.pos.z },
            hit.typeId,
            hit.stackPos,
        });
    }
    return out;
}

bool isSightClear(const MapPos from, const MapPos to)
{
    return g_map.isSightClear(toPosition(from), toPosition(to));
}

} // namespace tibia::game
