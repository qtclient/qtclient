#include "minimap/waypointsectors.h"

#include <gtest/gtest.h>

#include <QImage>

using tibia::minimap::TWaypointSector;
using tibia::minimap::TWaypointSectors;

TEST(WaypointSectors, UnknownWithoutSector)
{
    const TWaypointSectors sectors;
    EXPECT_EQ(sectors.costAt(Position(100, 200, 7)), TWaypointSector::UnknownCost);
}

TEST(WaypointSectors, SetCostAddsSectorOfUnknownCosts)
{
    TWaypointSectors sectors;
    sectors.setCost(Position(256 + 3, 512 + 5, 7), 90);
    ASSERT_EQ(sectors.sectors().size(), 1u);
    EXPECT_EQ(sectors.sectors().front().tileId, Position(256, 512, 7));
    EXPECT_EQ(sectors.costAt(Position(256 + 3, 512 + 5, 7)), 90);
    // The rest of the tile is unknown, and so is the same field on another floor.
    EXPECT_EQ(sectors.costAt(Position(256 + 5, 512 + 3, 7)), TWaypointSector::UnknownCost);
    EXPECT_EQ(sectors.costAt(Position(256 + 3, 512 + 5, 6)), TWaypointSector::UnknownCost);
    // A second cost in the same tile keeps the one sector.
    sectors.setCost(Position(256 + 255, 512 + 255, 7), 120);
    EXPECT_EQ(sectors.sectors().size(), 1u);
    EXPECT_EQ(sectors.costAt(Position(256 + 255, 512 + 255, 7)), 120);
}

TEST(WaypointSectors, SetSectorCopiesTheWaypointImageRowByRow)
{
    QImage waypoints(TWaypointSector::Size, TWaypointSector::Size, QImage::Format_Indexed8);
    waypoints.fill(0);
    waypoints.scanLine(2)[7] = 150;
    TWaypointSectors sectors;
    sectors.setCost(Position(0, 0, 7), 33);
    sectors.setSector(Position(0, 0, 7), waypoints);
    ASSERT_EQ(sectors.sectors().size(), 1u);
    EXPECT_EQ(sectors.costAt(Position(7, 2, 7)), 150);
    EXPECT_EQ(sectors.costAt(Position(0, 0, 7)), 0);
}

TEST(WaypointSectors, RemoveSectorsDropsOnlyTheGivenTiles)
{
    TWaypointSectors sectors;
    sectors.setCost(Position(10, 10, 7), 1);
    sectors.setCost(Position(300, 10, 7), 2);
    sectors.setCost(Position(10, 300, 7), 3);
    sectors.removeSectors({ Position(256, 0, 7), Position(0, 256, 7) });
    ASSERT_EQ(sectors.sectors().size(), 1u);
    EXPECT_EQ(sectors.costAt(Position(10, 10, 7)), 1);
    EXPECT_EQ(sectors.costAt(Position(300, 10, 7)), TWaypointSector::UnknownCost);
}
