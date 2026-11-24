#include "minimapworldfeed.h"

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

// CIP: the world storage's stored volume on floor `z` (one field per floor
// of perspective away from its shear floor).
QRect TMinimapWorldFeed::areaOn(const int z) const
{
    const tibia::worldmap::TWorldMapCuboid area = tibia::worldmap::TWorldMapStorage::ensureStorage()->storedVolume().shearedFloorArea(z);
    return QRect(area.origin.x, area.origin.y, area.extent.width, area.extent.height);
}

}
