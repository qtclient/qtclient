#include <gtest/gtest.h>

#include "worldmap/coordinates.h"

using tibia::worldmap::TShearedWorldMapCuboid;
using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapCuboid;
using tibia::worldmap::TWorldMapExtent;

TEST(WorldMapCoordinateOps, ArithmeticWithExtents)
{
    const TWorldMapCoordinate a(100, 200, 7);
    EXPECT_EQ(a + TWorldMapExtent(3, -2, 1), TWorldMapCoordinate(103, 198, 8));
    EXPECT_EQ(a - TWorldMapExtent(0, 0, 1), TWorldMapCoordinate(100, 200, 6));
    EXPECT_EQ(TWorldMapCoordinate(105, 190, 9) - a, TWorldMapExtent(5, -10, 2));
    EXPECT_EQ(minimum(a, TWorldMapCoordinate(90, 210, 7)), TWorldMapCoordinate(90, 200, 7));
    EXPECT_EQ(maximum(a, TWorldMapCoordinate(90, 210, 8)), TWorldMapCoordinate(100, 210, 8));
    EXPECT_EQ(TWorldMapExtent(-3, 4, -1).absolute(), TWorldMapExtent(3, 4, 1));
}

TEST(WorldMapCuboidOps, Constructors)
{
    const TWorldMapCuboid single = TWorldMapCuboid::ofField({ 10, 20, 7 });
    EXPECT_EQ(single.getExtent(), TWorldMapExtent(1, 1, 1));
    EXPECT_EQ(single.getEnd(), TWorldMapCoordinate(11, 21, 8));
    const TWorldMapCuboid flat({ 10, 20, 7 }, 18, 14);
    EXPECT_EQ(flat.getExtent(), TWorldMapExtent(18, 14, 1));
    EXPECT_EQ(flat.getOrigin(), TWorldMapCoordinate(10, 20, 7));
    EXPECT_EQ(TWorldMapCuboid({ 1, 2, 3 }, TWorldMapExtent(4, 5, 6)).lastFloorOrigin(), TWorldMapCoordinate(1, 2, 8));
}

TEST(WorldMapCuboidOps, IntersectionAndUnion)
{
    const TWorldMapCuboid a({ 0, 0, 0 }, TWorldMapExtent(10, 10, 2));
    const TWorldMapCuboid b({ 5, 6, 1 }, TWorldMapExtent(10, 10, 5));
    EXPECT_EQ(a.intersected(b), TWorldMapCuboid({ 5, 6, 1 }, TWorldMapExtent(5, 4, 1)));
    EXPECT_EQ(a.united(b), TWorldMapCuboid({ 0, 0, 0 }, TWorldMapExtent(15, 16, 6)));
    // Touching boxes overlap in an empty box at the seam; apart, the result is the empty box at 0.
    EXPECT_EQ(a.intersected(TWorldMapCuboid({ 10, 0, 0 }, TWorldMapExtent(3, 3, 1))),
              TWorldMapCuboid({ 10, 0, 0 }, TWorldMapExtent(0, 3, 1)));
    EXPECT_EQ(a.intersected(TWorldMapCuboid({ 20, 0, 0 }, TWorldMapExtent(3, 3, 1))),
              TWorldMapCuboid({ 0, 0, 0 }, TWorldMapExtent(0, 0, 0)));
}

TEST(WorldMapCuboidOps, FloorAreaAndShearEnvelope)
{
    const TWorldMapCuboid box({ 100, 100, 5 }, TWorldMapExtent(18, 14, 3));
    EXPECT_EQ(box.floorArea(7), TWorldMapCuboid({ 98, 98, 7 }, TWorldMapExtent(18, 14, 1)));
    EXPECT_EQ(box.shearEnvelope(), TWorldMapCuboid({ 97, 97, 5 }, TWorldMapExtent(24, 20, 3)));
}

TEST(WorldMapCuboidOps, ShearedConstructionAndUnshear)
{
    const TShearedWorldMapCuboid c = TShearedWorldMapCuboid::fromShearOrigin({ 100, 100, 7 }, TWorldMapExtent(18, 14, 8), 0);
    EXPECT_EQ(c.shearFloor, 7);
    EXPECT_EQ(c.origin, TWorldMapCoordinate(100, 100, 0));
    EXPECT_EQ(c.extent, TWorldMapExtent(18, 14, 8));
    // First floor below the shear floor: the empty box.
    const TShearedWorldMapCuboid bad = TShearedWorldMapCuboid::fromShearOrigin({ 100, 100, 3 }, TWorldMapExtent(18, 14, 8), 5);
    EXPECT_EQ(bad.shearFloor, 0);
    EXPECT_EQ(bad.extent, TWorldMapExtent(0, 0, 0));

    const TShearedWorldMapCuboid u = c.unsheared();
    EXPECT_EQ(u.origin, TWorldMapCoordinate(107, 107, 0));
    EXPECT_EQ(u.shearFloor, 0);
    EXPECT_EQ(u.extent, c.extent);
}

using tibia::worldmap::TWorldMapSubfieldCoordinate;
using tibia::worldmap::TWorldMapSubfieldExtent;

TEST(WorldMapSubfieldOps, OffsetsAndDifferences)
{
    const TWorldMapSubfieldCoordinate a(32 * 100 + 7, 32 * 50 + 31, 7);
    EXPECT_EQ(TWorldMapSubfieldExtent::offsetInField(a), TWorldMapSubfieldExtent(7, 31, 0));
    EXPECT_EQ(a + TWorldMapSubfieldExtent(1, -1, 0), TWorldMapSubfieldCoordinate(32 * 100 + 8, 32 * 50 + 30, 7));
    EXPECT_EQ(a - TWorldMapSubfieldCoordinate(32 * 100, 32 * 50, 6), TWorldMapSubfieldExtent(7, 31, 1));
    // Offset 16/16 with elevation 8 lands 8 subfields up-left of the field's centre.
    EXPECT_EQ(subfieldOfField({ 100, 50, 7 }, TWorldMapSubfieldExtent(16, 16, 8)),
              TWorldMapSubfieldCoordinate(3208, 1608, 7));
}
