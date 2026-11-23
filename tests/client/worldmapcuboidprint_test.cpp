// CIP worldmap::TWorldMapCuboid: the debug prints and the field at an index.
#include "worldmap/coordinates.h"

#include <QDebug>
#include <QString>

#include <gtest/gtest.h>

#include <sstream>

using namespace tibia::worldmap;

TEST(WorldMapCuboidPrint, StreamPrintsOriginEndAndExtent)
{
    const TWorldMapCuboid cuboid({ 10, 20, 5 }, TWorldMapExtent(18, 14, 3));
    std::ostringstream stream;
    stream << cuboid;
    EXPECT_EQ(stream.str(), "TWorldMapCuboid([10,20,5] - [28, 34, 8], Extent[18, 14, 3])");
}

TEST(WorldMapCuboidPrint, QDebugPrintsWithoutAutomaticSpaces)
{
    const TWorldMapCuboid cuboid({ 1, 2, 7 }, TWorldMapExtent(3, 4, 1));
    QString text;
    QDebug(&text) << cuboid;
    // Only the literal separators: no automatic spaces between the parts.
    EXPECT_EQ(text.trimmed(), QStringLiteral("TWorldMapCuboid([1,2,7] - [4, 6, 8], Extent[3, 4, 1])"));
}

TEST(WorldMapCuboidIndex, ColumnFirstThenFloorsWithShear)
{
    const TWorldMapCuboid cuboid({ 100, 200, 6 }, TWorldMapExtent(3, 2, 2));
    // y runs fastest.
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(0), TWorldMapCoordinate(100, 200, 6));
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(1), TWorldMapCoordinate(100, 201, 6));
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(2), TWorldMapCoordinate(101, 200, 6));
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(5), TWorldMapCoordinate(102, 201, 6));
    // The next floor down, moved one field up-left.
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(6), TWorldMapCoordinate(99, 199, 7));
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(11), TWorldMapCoordinate(101, 200, 7));
    // Past the last floor it wraps.
    EXPECT_EQ(cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(12), TWorldMapCoordinate(100, 200, 6));
}

TEST(ShearedWorldMapCuboidPrint, QDebugWrapsTheBoxAndShearFloor)
{
    const TShearedWorldMapCuboid cuboid(TWorldMapCoordinate(1, 2, 5), TWorldMapExtent(3, 4, 2), 7);
    QString text;
    QDebug(&text) << cuboid;
    EXPECT_EQ(text.trimmed(),
              QStringLiteral("TShearedWorldMapCuboid(TWorldMapCuboid([1,2,5] - [4, 6, 7], Extent[3, 4, 2]),Center Layer 7)"));
}

TEST(ShearedWorldMapCuboid, EqualNeedsTheSameShearFloor)
{
    const TShearedWorldMapCuboid a(TWorldMapCoordinate(1, 2, 5), TWorldMapExtent(3, 4, 2), 7);
    EXPECT_TRUE(a == TShearedWorldMapCuboid(TWorldMapCoordinate(1, 2, 5), TWorldMapExtent(3, 4, 2), 7));
    EXPECT_FALSE(a == TShearedWorldMapCuboid(TWorldMapCoordinate(1, 2, 5), TWorldMapExtent(3, 4, 2), 6));
    EXPECT_FALSE(a == TShearedWorldMapCuboid(TWorldMapCoordinate(1, 2, 5), TWorldMapExtent(3, 4, 1), 7));
}
