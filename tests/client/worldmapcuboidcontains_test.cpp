#include <gtest/gtest.h>

#include "worldmap/coordinates.h"

using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapCuboid;
using tibia::worldmap::TWorldMapExtent;

// A box of 3 x 2 fields on floors 6 and 7, starting at (100, 200, 6).
TEST(WorldMapCuboidContainsSheared, FirstFloorIsHalfOpen)
{
    const TWorldMapCuboid box({ 100, 200, 6 }, TWorldMapExtent(3, 2, 2));
    EXPECT_TRUE(box.containsSheared({ 100, 200, 6 }));
    EXPECT_TRUE(box.containsSheared({ 102, 201, 6 }));
    EXPECT_FALSE(box.containsSheared({ 103, 200, 6 }));
    EXPECT_FALSE(box.containsSheared({ 100, 202, 6 }));
    EXPECT_FALSE(box.containsSheared({ 99, 200, 6 }));
    EXPECT_FALSE(box.containsSheared({ 100, 199, 6 }));
}

TEST(WorldMapCuboidContainsSheared, LowerFloorsShiftOneFieldPerFloor)
{
    const TWorldMapCuboid box({ 100, 200, 6 }, TWorldMapExtent(3, 2, 2));
    // On floor 7 a field counts as one field further down-right.
    EXPECT_TRUE(box.containsSheared({ 99, 199, 7 }));
    EXPECT_TRUE(box.containsSheared({ 101, 200, 7 }));
    EXPECT_FALSE(box.containsSheared({ 102, 200, 7 }));
    EXPECT_FALSE(box.containsSheared({ 100, 201, 7 }));
}

TEST(WorldMapCuboidContainsSheared, FloorsOutsideTheBox)
{
    const TWorldMapCuboid box({ 100, 200, 6 }, TWorldMapExtent(3, 2, 2));
    EXPECT_FALSE(box.containsSheared({ 98, 198, 8 }));
    EXPECT_FALSE(box.containsSheared({ 101, 201, 5 }));
}
