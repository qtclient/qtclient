#include "worldmap/worldmapstorage.h"

#include "worldmap/worldmapfield.h"

#include <tibia/world/map.h>
#include <tibia/world/world.h>

namespace tibia::worldmap {

namespace {

quint64 pack(const Position& pos)
{
    return (static_cast<quint64>(static_cast<uint32_t>(pos.x)) << 32)
        | (static_cast<quint64>(static_cast<uint16_t>(pos.y)) << 16)
        | static_cast<quint64>(pos.z);
}

}

TWorldMapStorage::TWorldMapStorage(QObject* parent)
    : cip::tibia::worldmap::TWorldMapStorage(parent)
    , m_lookupMembers(new TCacheForWorldmapStorageLookupMembers(this))
{
}

TWorldMapStorage::~TWorldMapStorage() = default;

quint64 TWorldMapStorage::key(const Position& pos)
{
    return pack(pos);
}

TWorldMapField* TWorldMapStorage::fieldAt(const TWorldMapCoordinate& coordinate)
{
    return fieldAt(coordinate.toPosition());
}

TWorldMapField* TWorldMapStorage::fieldAt(const Position& position)
{
    const quint64 id = key(position);
    if (TWorldMapField* existing = m_fields.value(id))
        return existing;
    auto* field = new TWorldMapField(this);
    field->setCoordinate(TWorldMapCoordinate(position));
    field->setTile(tibia::world::fields().getTile(position));
    m_fields.insert(id, field);
    return field;
}

void TWorldMapStorage::notifyCoordinateChanged(const Position& position)
{
    if (TWorldMapField* field = m_fields.value(key(position))) {
        field->setTile(tibia::world::fields().getTile(position));
        field->notifyAppearanceInstancesChanged();
    }
    Q_EMIT informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate(position));
}

void TWorldMapStorage::notifyStoredVolumeChanged()
{
    Q_EMIT storedVolumeHasChanged();
}

}
