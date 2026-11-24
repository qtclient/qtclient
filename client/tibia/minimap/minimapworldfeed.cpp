#include "minimapworldfeed.h"

#include <tibia/game/mapsession.h>

#include <algorithm>
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

namespace tibia::minimap {

TMinimapWorldFeed::TMinimapWorldFeed(TMinimapTileStorage& storage)
    : m_storage(storage)
{
    m_storage.setWorld(this);
}

TMinimapWorldFeed::~TMinimapWorldFeed()
{
    m_storage.setWorld(nullptr);
}

bool TMinimapWorldFeed::isKnown(const Position& coord) const
{
    tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(coord);
    return tile && !tile->isEmpty();
}

// The world storage's minimap data of the field (its defaults without one).
TMinimapDataOfOneField TMinimapWorldFeed::fieldData(const Position& coord) const
{
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->minimapDataOfField(
        tibia::worldmap::TWorldMapCoordinate(coord));
}

void TMinimapWorldFeed::forEachKnownField(const int z, const std::function<void(const Position&)>& visit) const
{
    for (tibia::worldmap::TWorldMapField* tile : tibia::worldmap::fieldsOnFloor(static_cast<int8_t>(z))) {
        if (!tile->isEmpty())
            visit(tile->getPosition());
    }
}

// CIP: the world area shifted one field per floor of perspective .
QRect TMinimapWorldFeed::areaOn(const int z) const
{
    const tibia::game::MapPos mapCenter = tibia::game::centralPosition();
    const Position center(mapCenter.x, mapCenter.y, static_cast<uint8_t>(mapCenter.z));
    const tibia::game::MapAwareRange range = tibia::game::awareRange();
    const int shift = center.z - z;
    return QRect(center.x - range.left + shift, center.y - range.top + shift, range.horizontal(),
                 range.vertical());
}

}
