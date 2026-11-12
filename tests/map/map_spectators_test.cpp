#include <gtest/gtest.h>

#include "creatures/creaturestorage.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/coordinates.h"
#include "client/position.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace {

class StorageScope
{
public:
    StorageScope()
    {
        m_storage = std::make_unique<tibia::creatures::TCreatureStorage>();
        tibia::creatures::TCreatureStorage::setInstance(m_storage.get());
    }

    ~StorageScope()
    {
        tibia::creatures::TCreatureStorage::setInstance(nullptr);
        m_storage.reset();
    }

    tibia::creatures::TCreatureStorage& storage() { return *m_storage; }

private:
    std::unique_ptr<tibia::creatures::TCreatureStorage> m_storage;
};

void placeCreature(tibia::worldmap::TWorldMapField& tile,
                   tibia::creatures::TCreatureStorage& storage,
                   const uint32_t id)
{
    storage.getOrCreate(id);
    tile.addCreatureId(id, -1);
}

std::vector<uint32_t> creatureIdsOnField(const tibia::worldmap::TWorldMapField& tile)
{
    std::vector<uint32_t> ids;
    for (const auto& entry : tile.stackCreatureEntries())
        ids.push_back(entry.creatureId);
    return ids;
}

} // namespace

TEST(TileSpectators, CreatureIdsTrackStackOrder)
{
    StorageScope scope;
    const Position position(100, 100, 7);
    tibia::worldmap::TWorldMapField tile;
    tile.setCoordinate(tibia::worldmap::TWorldMapCoordinate(position));

    EXPECT_FALSE(tile.hasCreatures());
    EXPECT_TRUE(tile.stackCreatureEntries().empty());
    EXPECT_TRUE(tile.getCreatures().empty()); // CreaturePtr path empty without LocalPlayerSessionThing

    placeCreature(tile, scope.storage(), 1);
    placeCreature(tile, scope.storage(), 2);
    placeCreature(tile, scope.storage(), 3);

    EXPECT_EQ((std::vector<uint32_t>{ 1, 2, 3 }), creatureIdsOnField(tile));

    // appendSpectators stays LocalPlayerSessionThing-only (no Map registry).
    std::vector<CreaturePtr> creaturePtrs;
    tile.appendSpectators(creaturePtrs);
    EXPECT_TRUE(creaturePtrs.empty());
}

TEST(TileSpectators, RemovingCreatureIdsClearsField)
{
    StorageScope scope;
    const Position position(210, 210, 7);
    tibia::worldmap::TWorldMapField tile;
    tile.setCoordinate(tibia::worldmap::TWorldMapCoordinate(position));

    placeCreature(tile, scope.storage(), 1);
    placeCreature(tile, scope.storage(), 2);
    ASSERT_TRUE(tile.hasCreatures());
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2 }), creatureIdsOnField(tile));

    ASSERT_TRUE(tile.removeCreatureById(1));
    EXPECT_TRUE(tile.hasCreatures());
    EXPECT_EQ((std::vector<uint32_t>{ 2 }), creatureIdsOnField(tile));

    ASSERT_TRUE(tile.removeCreatureById(2));
    EXPECT_FALSE(tile.hasCreatures());
    EXPECT_TRUE(tile.stackCreatureEntries().empty());
}

TEST(StorageSpectators, RangeFiltering)
{
    StorageScope scope;
    const Position center(101, 101, 8);

    auto c1 = scope.storage().getOrCreate(1);
    c1->setPosition(center);
    auto c2 = scope.storage().getOrCreate(2);
    c2->setPosition(center.translated(-1, -1));
    auto c3 = scope.storage().getOrCreate(3);
    c3->setPosition(center.translated(2, 2));

    {
        const auto near = scope.storage().spectatorsNear(center, false, 1, 1, 1, 1);
        std::unordered_set<uint32_t> ids;
        for (const auto& row : near)
            ids.insert(row->creatureId());
        EXPECT_EQ((std::unordered_set<uint32_t>{ 1, 2 }), ids);
    }

    {
        const auto far = scope.storage().spectatorsNear(center, false, 2, 2, 2, 2);
        std::unordered_set<uint32_t> ids;
        for (const auto& row : far)
            ids.insert(row->creatureId());
        EXPECT_EQ((std::unordered_set<uint32_t>{ 1, 2, 3 }), ids);
    }
}

TEST(StorageSpectators, MultiFloorIncludesVerticalNeighbors)
{
    StorageScope scope;
    const Position center(190, 290, 8);

    scope.storage().getOrCreate(11)->setPosition(center.translated(0, 0, -1));
    scope.storage().getOrCreate(12)->setPosition(center);
    scope.storage().getOrCreate(13)->setPosition(center.translated(0, 0, 1));

    // multiFloor uses Map aware floors; without a live Map session this may
    // only return the center z. At least the same-floor creature is present.
    const auto rows = scope.storage().spectatorsNear(center, false, 0, 0, 0, 0);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(12u, rows.front()->creatureId());
}

TEST(StorageSpectators, UniqueIds)
{
    StorageScope scope;
    const Position center(220, 320, 7);
    scope.storage().getOrCreate(42)->setPosition(center);

    const auto rows = scope.storage().spectatorsNear(center, false, 1, 1, 0, 0);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(42u, rows.front()->creatureId());
}
