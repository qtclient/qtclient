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
    (void)tile.addObject(storage.getOrCreate(id)->placeholderAppearance());
}

int stackPositionOfCreature(const tibia::worldmap::TWorldMapField& tile, const uint32_t id)
{
    const auto& stack = tile.appearances();
    for (int i = 0; i < static_cast<int>(stack.size()); ++i) {
        if (stack[i]->creatureId() == id)
            return i;
    }
    return -1;
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
    tibia::worldmap::TWorldMapField tile(tibia::worldmap::TWorldMapCoordinate(position), 10);

    EXPECT_FALSE(tile.hasCreatures());
    EXPECT_TRUE(tile.stackCreatureEntries().empty());

    placeCreature(tile, scope.storage(), 1);
    placeCreature(tile, scope.storage(), 2);
    placeCreature(tile, scope.storage(), 3);

    EXPECT_EQ((std::vector<uint32_t>{ 1, 2, 3 }), creatureIdsOnField(tile));
}

TEST(TileSpectators, RemovingCreatureIdsClearsField)
{
    StorageScope scope;
    const Position position(210, 210, 7);
    tibia::worldmap::TWorldMapField tile(tibia::worldmap::TWorldMapCoordinate(position), 10);

    placeCreature(tile, scope.storage(), 1);
    placeCreature(tile, scope.storage(), 2);
    ASSERT_TRUE(tile.hasCreatures());
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2 }), creatureIdsOnField(tile));

    ASSERT_TRUE(tile.removeObject(static_cast<uint8_t>(stackPositionOfCreature(tile, 1))));
    EXPECT_TRUE(tile.hasCreatures());
    EXPECT_EQ((std::vector<uint32_t>{ 2 }), creatureIdsOnField(tile));

    ASSERT_TRUE(tile.removeObject(static_cast<uint8_t>(stackPositionOfCreature(tile, 2))));
    EXPECT_FALSE(tile.hasCreatures());
    EXPECT_TRUE(tile.stackCreatureEntries().empty());
}

TEST(TileSpectators, StackLimitRejectsAndDrops)
{
    StorageScope scope;
    tibia::worldmap::TWorldMapField tile(tibia::worldmap::TWorldMapCoordinate(Position(300, 300, 7)), 3);
    for (uint32_t id = 1; id <= 3; ++id)
        EXPECT_EQ(nullptr, tile.addObject(scope.storage().getOrCreate(id)->placeholderAppearance()));
    // A creature sorts after the creatures: past the limit the field does not
    // take it and hands it back, yet it counts as placed on a field.
    const auto fourth = scope.storage().getOrCreate(4);
    EXPECT_EQ(fourth->placeholderAppearance(), tile.addObject(fourth->placeholderAppearance()));
    EXPECT_TRUE(fourth->isPlacedOnField());
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2, 3 }), creatureIdsOnField(tile));
    // Inserted at the bottom, the last object drops off and keeps its flag.
    const auto fifth = scope.storage().getOrCreate(5);
    const auto dropped = tile.insertObject(fifth->placeholderAppearance(), 0);
    EXPECT_EQ(scope.storage().getOrCreate(3)->placeholderAppearance(), dropped);
    EXPECT_TRUE(scope.storage().getOrCreate(3)->isPlacedOnField());
    EXPECT_EQ((std::vector<uint32_t>{ 5, 1, 2 }), creatureIdsOnField(tile));
}

TEST(TileSpectators, SetObjectsReplacesTheStack)
{
    StorageScope scope;
    tibia::worldmap::TWorldMapField tile(tibia::worldmap::TWorldMapCoordinate(Position(310, 310, 7)), 2);
    const auto a = scope.storage().getOrCreate(1);
    const auto b = scope.storage().getOrCreate(2);
    const auto c = scope.storage().getOrCreate(3);
    placeCreature(tile, scope.storage(), 1);
    tile.setObjects({ b->placeholderAppearance(), c->placeholderAppearance(), a->placeholderAppearance() });
    // The old creature left the field first; the third object did not fit.
    EXPECT_EQ((std::vector<uint32_t>{ 2, 3 }), creatureIdsOnField(tile));
    EXPECT_TRUE(b->isPlacedOnField());
    EXPECT_TRUE(c->isPlacedOnField());
    EXPECT_EQ(2u, tile.drawList().size());
    // removeObject past the stack is null; replaceObject hands back what left.
    EXPECT_EQ(nullptr, tile.removeObject(5));
    EXPECT_EQ(c->placeholderAppearance(), tile.replaceObject(a->placeholderAppearance(), 1));
    EXPECT_FALSE(c->isPlacedOnField());
    EXPECT_EQ((std::vector<uint32_t>{ 2, 1 }), creatureIdsOnField(tile));
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
