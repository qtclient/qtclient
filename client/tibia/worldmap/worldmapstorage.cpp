#include "worldmap/worldmapstorage.h"

#include "worldmap/worldmapfield.h"

#include <tibia/world/map.h>
#include <tibia/world/world.h>

#include <memory>

namespace tibia::worldmap {

namespace {

TWorldMapStorage* g_worldMapStorage = nullptr;
std::unique_ptr<TWorldMapStorage> g_fallbackWorldMapStorage;

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

TWorldMapStorage::~TWorldMapStorage()
{
    if (g_worldMapStorage == this)
        g_worldMapStorage = nullptr;
}

TWorldMapStorage* TWorldMapStorage::instance()
{
    return g_worldMapStorage;
}

void TWorldMapStorage::setInstance(TWorldMapStorage* storage)
{
    g_worldMapStorage = storage;
}

TWorldMapStorage* TWorldMapStorage::ensureStorage()
{
    if (g_worldMapStorage)
        return g_worldMapStorage;
    if (!g_fallbackWorldMapStorage)
        g_fallbackWorldMapStorage = std::make_unique<TWorldMapStorage>();
    g_worldMapStorage = g_fallbackWorldMapStorage.get();
    return g_worldMapStorage;
}

void TWorldMapStorage::removeField(const Position& position)
{
    m_fields.remove(key(position));
}

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
    return tibia::world::fields().getTile(position);
}

TWorldMapField* TWorldMapStorage::getOrCreateField(const Position& position)
{
    const quint64 id = key(position);
    if (TWorldMapField* existing = m_fields.value(id)) {
        // Unaware scrub may have nulled the Map block slot while storage kept
        // the field — re-bind so getTile/clearField see the same identity.
        if (!tibia::world::fields().getTile(position))
            tibia::world::fields().setFieldInBlock(position, existing);
        return existing;
    }

    TWorldMapField* field = tibia::world::fields().getTile(position);
    if (!field) {
        field = new TWorldMapField(this);
        field->setCoordinate(TWorldMapCoordinate(position));
        tibia::world::fields().setFieldInBlock(position, field);
    } else if (field->parent() == nullptr) {
        field->setParent(this);
    }

    m_fields.insert(id, field);
    return field;
}

void TWorldMapStorage::notifyCoordinateChanged(const Position& position)
{
    if (TWorldMapField* field = fieldAt(position))
        field->notifyAppearanceInstancesChanged();
    Q_EMIT informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate(position));
}

TShearedWorldMapCuboid TWorldMapStorage::storedVolume() const
{
    const Position center = tibia::world::fields().getCentralPosition();
    const AwareRange range = tibia::world::fields().getAwareRange();
    return TShearedWorldMapCuboid(
        TWorldMapCoordinate(center.x - range.left, center.y - range.top, center.z),
        TWorldMapExtent(range.horizontal(), range.vertical(), 1));
}

void TWorldMapStorage::notifyStoredVolumeChanged()
{
    Q_EMIT storedVolumeHasChanged(storedVolume());
}

}
