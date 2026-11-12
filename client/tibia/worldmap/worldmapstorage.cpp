#include "worldmap/worldmapstorage.h"

#include "worldmap/worldmapfield.h"

#include "client/const.h"
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/mapsession.h>
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
    ensureSpatialFloorsSized();
    // Size missile floors only — full sessionState().init() calls resetAwareRange →
    // removeUnawareThings → ensureStorage(), which re-enters while g_worldMapStorage
    // is still null (stack overflow). Callers init after setInstance / ensureStorage.
    m_sessionState.ensureMissileFloorsSized();
}

void TWorldMapStorage::ensureSpatialFloorsSized()
{
    const auto maxZ = static_cast<size_t>(tibia::config::mapMaxZ()) + 1;
    if (m_spatialFloors.size() < maxZ)
        m_spatialFloors.resize(maxZ);
}

uint16_t TWorldMapStorage::blockIndex(const Position& pos)
{
    return static_cast<uint16_t>(((pos.y / BLOCK_SIZE) * (65536 / BLOCK_SIZE)) + (pos.x / BLOCK_SIZE));
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
    if (!g_fallbackWorldMapStorage) {
        g_fallbackWorldMapStorage = std::make_unique<TWorldMapStorage>();
        g_worldMapStorage = g_fallbackWorldMapStorage.get();
        // After ctor: instance is live, so session init (aware range) is safe.
        g_worldMapStorage->sessionState().init();
        return g_worldMapStorage;
    }
    g_worldMapStorage = g_fallbackWorldMapStorage.get();
    return g_worldMapStorage;
}

void TWorldMapStorage::removeField(const Position& position)
{
    m_fields.remove(key(position));
}

void TWorldMapStorage::clearAllFields()
{
    m_fields.clear();
    clearSpatialIndex();
}

void TWorldMapStorage::clearSpatialIndex()
{
    ensureSpatialFloorsSized();
    for (auto& blocks : m_spatialFloors)
        blocks.clear();
}

WorldMapFieldList TWorldMapStorage::spatialFieldList(const int8_t floor)
{
    WorldMapFieldList tiles;
    ensureSpatialFloorsSized();
    if (floor > tibia::config::mapMaxZ())
        return tiles;

    const auto collect = [&](const uint8_t z) {
        for (const auto& [_, block] : m_spatialFloors[z]) {
            for (const auto& tile : block.getTiles()) {
                if (tile != nullptr)
                    tiles.emplace_back(tile);
            }
        }
    };

    if (floor < 0) {
        for (auto z = -1; ++z <= tibia::config::mapMaxZ();)
            collect(static_cast<uint8_t>(z));
    } else {
        collect(static_cast<uint8_t>(floor));
    }
    return tiles;
}

TWorldMapField* TWorldMapStorage::fieldInSpatialIndex(const Position& position)
{
    if (!position.isMapPosition())
        return nullptr;
    ensureSpatialFloorsSized();
    auto& blocks = m_spatialFloors[position.z];
    const auto it = blocks.find(blockIndex(position));
    if (it != blocks.end())
        return it->second.get(position);
    return nullptr;
}

void TWorldMapStorage::bindFieldToSpatialIndex(const Position& position, TWorldMapField* field)
{
    if (!position.isMapPosition())
        return;
    ensureSpatialFloorsSized();
    m_spatialFloors[position.z][blockIndex(position)].assign(position, field);
}

void TWorldMapStorage::removeFieldFromSpatialIndex(const Position& position)
{
    if (!position.isMapPosition())
        return;
    ensureSpatialFloorsSized();
    auto& blocks = m_spatialFloors[position.z];
    const auto it = blocks.find(blockIndex(position));
    if (it != blocks.end())
        it->second.remove(position);
}

void TWorldMapStorage::pruneUnawareSpatialFields(
    const std::function<bool(const Position&)>& isAware,
    const std::function<void(const Position&)>& onRemoved)
{
    ensureSpatialFloorsSized();
    for (auto z = -1; ++z <= tibia::config::mapMaxZ();) {
        auto& tileBlocks = m_spatialFloors[static_cast<size_t>(z)];
        for (auto it = tileBlocks.begin(); it != tileBlocks.end();) {
            auto& block = it->second;
            bool blockEmpty = true;
            for (const auto& tile : block.getTiles()) {
                if (!tile)
                    continue;

                const Position& pos = tile->getPosition();
                if (isAware(pos)) {
                    blockEmpty = false;
                    continue;
                }

                if (!tile->isEmpty() || tile->hasEffect())
                    tile->clean();

                block.remove(pos);
                removeField(pos);
                onRemoved(pos);
            }

            if (blockEmpty)
                it = tileBlocks.erase(it);
            else
                ++it;
        }
    }
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
    return m_fields.value(key(position));
}

TWorldMapField* TWorldMapStorage::getOrCreateField(const Position& position)
{
    const quint64 id = key(position);
    if (TWorldMapField* existing = m_fields.value(id)) {
        // Unaware scrub may have nulled the Map block slot while storage kept
        // the field — re-bind so pathfind iterators and clearField agree.
        if (!fieldInSpatialIndex(position))
            bindFieldToSpatialIndex(position, existing);
        return existing;
    }

    if (TWorldMapField* orphan = fieldInSpatialIndex(position)) {
        if (orphan->parent() == nullptr)
            orphan->setParent(this);
        m_fields.insert(id, orphan);
        return orphan;
    }

    auto* field = new TWorldMapField(this);
    field->setCoordinate(TWorldMapCoordinate(position));
    bindFieldToSpatialIndex(position, field);
    m_fields.insert(id, field);
    return field;
}

void TWorldMapStorage::clearField(const Position& position)
{
    // Map::cleanTile → notificateTileUpdate → notifyCoordinateChanged when live.
    sessionState().cleanTile(position);
}

bool TWorldMapStorage::placeAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance,
                                       const Position& position, const int stackPos)
{
    return sessionState().placeAppearance(appearance, position, stackPos);
}

bool TWorldMapStorage::placeCreatureId(const uint32_t creatureId, const Position& position, const int stackPos)
{
    return sessionState().placeCreatureId(creatureId, position, stackPos);
}

bool TWorldMapStorage::removeCreatureFromFields(const uint32_t creatureId, const Position& fieldPos)
{
    return sessionState().removeCreatureFromFields(creatureId, fieldPos);
}

bool TWorldMapStorage::applyCreatureMove(const uint32_t creatureId, const Position& oldPos, const int oldStack,
                                         const Position& newPos, const bool byId)
{
    return sessionState().applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId);
}

void TWorldMapStorage::notifyCoordinateChanged(const Position& position)
{
    if (TWorldMapField* field = fieldAt(position))
        field->notifyAppearanceInstancesChanged();
    Q_EMIT informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate(position));
}

TShearedWorldMapCuboid TWorldMapStorage::storedVolume() const
{
    const tibia::game::MapPos center = tibia::game::centralPosition();
    const tibia::game::MapAwareRange range = tibia::game::awareRange();
    return TShearedWorldMapCuboid(
        TWorldMapCoordinate(center.x - range.left, center.y - range.top, center.z),
        TWorldMapExtent(range.horizontal(), range.vertical(), 1));
}

void TWorldMapStorage::notifyStoredVolumeChanged()
{
    Q_EMIT storedVolumeHasChanged(storedVolume());
}

void TWorldMapStorage::notifyWindowChanged(const bool fullDescription) const
{
    sessionState().notificateWindowChanged(fullDescription);
}

void TWorldMapStorage::notifyTileUpdate(const Position& position, const std::shared_ptr<Thing>& thing,
                                        const tibia::client::Operation operation)
{
    sessionState().notificateTileUpdate(position, thing, operation);
}

void TWorldMapStorage::addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance)
{
    sessionState().addMissileAppearance(appearance);
}

bool TWorldMapStorage::removeMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance)
{
    return sessionState().removeMissileAppearance(appearance);
}

void TWorldMapStorage::addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance,
                                           const Position& position)
{
    sessionState().addEffectAppearance(appearance, position);
}

void TWorldMapStorage::addStaticText(const StaticTextPtr& text, const Position& position)
{
    sessionState().addStaticText(text, position);
}

bool TWorldMapStorage::removeStaticText(const StaticTextPtr& text)
{
    return sessionState().removeStaticText(text);
}

void TWorldMapStorage::addAnimatedText(const AnimatedTextPtr& text, const Position& position)
{
    sessionState().addAnimatedText(text, position);
}

bool TWorldMapStorage::removeAnimatedText(const AnimatedTextPtr& text)
{
    return sessionState().removeAnimatedText(text);
}

WorldMapFieldList fieldsOnFloor(const int8_t floor)
{
    return TWorldMapStorage::ensureStorage()->spatialFieldList(floor);
}

const std::vector<tibia::appearances::TAppearanceInstancePtr>& floorMissiles(const uint8_t z)
{
    return TWorldMapStorage::ensureStorage()->sessionState().getFloorMissiles(z);
}

}
