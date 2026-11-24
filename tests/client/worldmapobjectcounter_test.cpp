#include <gtest/gtest.h>

#include "worldmap/worldmapobjectcounter.h"

using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapObjectCounter;

namespace {

struct FloorDistance
{
    explicit FloorDistance(const int value)
        : previous(TWorldMapObjectCounter::floorDistance())
    {
        TWorldMapObjectCounter::setFloorDistance(value);
    }
    ~FloorDistance() { TWorldMapObjectCounter::setFloorDistance(previous); }
    int previous;
};

}

TEST(WorldMapObjectCounter, EmptyTypeHasNoObjectsAndNearestTen)
{
    const TWorldMapObjectCounter counter;
    int nearest = -1;
    EXPECT_EQ(counter.count(100, 9, nearest), 0u);
    EXPECT_EQ(nearest, 10);
}

TEST(WorldMapObjectCounter, CountsByChebyshevRingWithinMaxDistance)
{
    const FloorDistance floors(3);
    TWorldMapObjectCounter counter;
    counter.setCentre(TWorldMapCoordinate(100, 100, 7));
    counter.addObject(TWorldMapCoordinate(102, 101, 7), 500); // ring 2
    counter.addObject(TWorldMapCoordinate(97, 100, 7), 500);  // ring 3
    counter.addObject(TWorldMapCoordinate(105, 95, 7), 500);  // ring 5
    counter.addObject(TWorldMapCoordinate(110, 100, 7), 500); // ring 10: not kept

    int nearest = -1;
    EXPECT_EQ(counter.count(500, 9, nearest), 3u);
    EXPECT_EQ(nearest, 2);
    EXPECT_EQ(counter.count(500, 3, nearest), 2u);
    EXPECT_EQ(nearest, 2);
    EXPECT_EQ(counter.count(500, 1, nearest), 0u);
    EXPECT_EQ(nearest, 10);
    // A negative distance keeps no ring.
    EXPECT_EQ(counter.count(500, -1, nearest), 0u);
}

TEST(WorldMapObjectCounter, AboveGroundOtherFloorsAddFloorDistancePerFloor)
{
    const FloorDistance floors(3);
    TWorldMapObjectCounter counter;
    counter.setCentre(TWorldMapCoordinate(100, 100, 7));
    counter.addObject(TWorldMapCoordinate(101, 100, 6), 42); // 3 * 1 + 1 = ring 4
    counter.addObject(TWorldMapCoordinate(100, 100, 4), 42); // 3 * 3 + 0 = ring 9
    counter.addObject(TWorldMapCoordinate(101, 100, 4), 42); // ring 10: not kept
    int nearest = -1;
    EXPECT_EQ(counter.count(42, 9, nearest), 2u);
    EXPECT_EQ(nearest, 4);
}

TEST(WorldMapObjectCounter, UndergroundOnlyTheCentresFloorCounts)
{
    const FloorDistance floors(3);
    TWorldMapObjectCounter counter;
    counter.setCentre(TWorldMapCoordinate(100, 100, 8));
    counter.addObject(TWorldMapCoordinate(100, 100, 9), 42);
    counter.addObject(TWorldMapCoordinate(100, 101, 8), 42);
    int nearest = -1;
    EXPECT_EQ(counter.count(42, 9, nearest), 1u);
    EXPECT_EQ(nearest, 1);
}

TEST(WorldMapObjectCounter, RingSaturatesAt255)
{
    TWorldMapObjectCounter counter;
    for (int i = 0; i < 300; ++i)
        counter.addObject(TWorldMapCoordinate(0, 0, 0), 7);
    int nearest = -1;
    EXPECT_EQ(counter.count(7, 9, nearest), 255u);
    EXPECT_EQ(nearest, 0);
}

TEST(WorldMapObjectCounter, ResetClearsEveryAddedType)
{
    TWorldMapObjectCounter counter;
    counter.addObject(TWorldMapCoordinate(0, 0, 0), 7);
    counter.addObject(TWorldMapCoordinate(1, 0, 0), 9);
    counter.reset();
    int nearest = -1;
    EXPECT_EQ(counter.count(7, 9, nearest), 0u);
    EXPECT_EQ(counter.count(9, 9, nearest), 0u);
    counter.addObject(TWorldMapCoordinate(0, 0, 0), 9);
    EXPECT_EQ(counter.count(9, 9, nearest), 1u);
}
