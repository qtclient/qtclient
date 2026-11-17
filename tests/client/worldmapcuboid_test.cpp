// CIP worldmap::TWorldMapCoordinate helpers, TWorldMapCuboid and TShearedWorldMapCuboid.
#include "worldmap/coordinates.h"

#include <gtest/gtest.h>

using namespace tibia::worldmap;

TEST(WorldMapCoordinate, ShearsOntoAnotherFloor)
{
    const TWorldMapCoordinate field(100, 200, 7);
    EXPECT_EQ(field.shearedToFloor(5), TWorldMapCoordinate(102, 202, 5));
    EXPECT_EQ(field.shiftedTowardsFloor(9), TWorldMapCoordinate(102, 202, 7));
    EXPECT_EQ(field.shiftedAwayFromFloor(9), TWorldMapCoordinate(98, 198, 7));
}

TEST(WorldMapSubfieldCoordinate, FieldDividesTowardZero)
{
    EXPECT_EQ(TWorldMapSubfieldCoordinate(65, 31, 7).toFieldCoordinate(), TWorldMapCoordinate(2, 0, 7));
    EXPECT_EQ(TWorldMapSubfieldCoordinate(-1, -33, 7).toFieldCoordinate(), TWorldMapCoordinate(0, -1, 7));
}

TEST(WorldMapCuboid, EndIsOriginPlusExtent)
{
    const TWorldMapCuboid cuboid({ 10, 20, 5 }, TWorldMapExtent(18, 14, 3));
    EXPECT_EQ(cuboid.end, TWorldMapCoordinate(28, 34, 8));
    EXPECT_EQ(cuboid.center(), TWorldMapCoordinate(19, 27, 5));
    const TWorldMapCuboid moved = cuboid.translated(TWorldMapExtent(1, -2, 1));
    EXPECT_EQ(moved.origin, TWorldMapCoordinate(11, 18, 6));
    EXPECT_EQ(moved.end, TWorldMapCoordinate(29, 32, 9));
    EXPECT_EQ(TWorldMapCuboid().extent, TWorldMapExtent(0, 0, 0));
}

TEST(ShearedWorldMapCuboid, RestrictsToFloorsKeepingTheShear)
{
    const TShearedWorldMapCuboid volume({ 10, 20, 0 }, TWorldMapExtent(18, 14, 16), 7);
    const TShearedWorldMapCuboid floors = volume.restrictedToFloors(5, 8);
    EXPECT_EQ(floors.origin, TWorldMapCoordinate(10, 20, 5));
    EXPECT_EQ(floors.extent, TWorldMapExtent(18, 14, 4));
    EXPECT_EQ(floors.end, TWorldMapCoordinate(28, 34, 9));
    EXPECT_EQ(floors.shearFloor, 7);
    EXPECT_EQ(volume.restrictedToFloors(8, 5), TShearedWorldMapCuboid());
    EXPECT_EQ(TShearedWorldMapCuboid(TWorldMapCuboid({ 1, 2, 6 }, TWorldMapExtent(1, 1, 1))).shearFloor, 6);
}

TEST(ShearedWorldMapCuboid, ContainsFieldsSeenFromTheShearFloor)
{
    using tibia::worldmap::TShearedWorldMapCuboid;
    using tibia::worldmap::TWorldMapCoordinate;
    using tibia::worldmap::TWorldMapExtent;
    const TShearedWorldMapCuboid volume(TWorldMapCoordinate(92, 94, 0), TWorldMapExtent(18, 14, 8), 7);
    EXPECT_TRUE(volume.contains(TWorldMapCoordinate(92, 94, 7)));
    EXPECT_FALSE(volume.contains(TWorldMapCoordinate(110, 94, 7)));
    // One floor up the box is one field further south-east.
    EXPECT_FALSE(volume.contains(TWorldMapCoordinate(92, 94, 6)));
    EXPECT_TRUE(volume.contains(TWorldMapCoordinate(93, 95, 6)));
    EXPECT_FALSE(volume.contains(TWorldMapCoordinate(93, 95, 8)));
}
