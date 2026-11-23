// CIP skill wheel grid configuration: the fixed tile data, the revelation stages and the tile orders.

#include "skillwheel/gridconfiguration.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>

using tibia::skillwheel::gridConfiguration;

TEST(GridConfiguration, MaxPointsGrowFromTheCentre)
{
    const auto& grid = gridConfiguration();
    EXPECT_EQ(grid.maxPoints(0), 200);
    EXPECT_EQ(grid.maxPoints(1), 150);
    EXPECT_EQ(grid.maxPoints(8), 75);
    EXPECT_EQ(grid.maxPoints(14), 50);
    EXPECT_EQ(grid.maxPoints(21), 50);
    EXPECT_EQ(grid.maxPoints(35), 200);
    int total = 0;
    for (int tile = 0; tile < 36; ++tile)
        total += grid.maxPoints(tile);
    EXPECT_EQ(total, 4000);
}

TEST(GridConfiguration, PerksByTileAndVocation)
{
    const auto& grid = gridConfiguration();
    EXPECT_EQ(grid.smallPerk(0, 0), 2);
    EXPECT_EQ(grid.smallPerk(1, 4), 4);
    EXPECT_EQ(grid.smallPerk(14, 2), 3);
    EXPECT_EQ(grid.mediumPerk(0, 0), 8);
    EXPECT_EQ(grid.mediumPerk(0, 4), 39);
    EXPECT_EQ(grid.mediumPerk(7, 3), 46);
    EXPECT_EQ(grid.mediumPerk(35, 2), 23);
    EXPECT_EQ(grid.largePerk(0, 3), 0);
    EXPECT_EQ(grid.largePerk(1, 3), 11);
    EXPECT_EQ(grid.largePerk(3, 4), 15);
}

TEST(GridConfiguration, OutOfRangeThrows)
{
    const auto& grid = gridConfiguration();
    EXPECT_THROW(grid.maxPoints(36), std::out_of_range);
    EXPECT_THROW(grid.maxPoints(-1), std::out_of_range);
    EXPECT_THROW(grid.smallPerk(0, 5), std::out_of_range);
    EXPECT_THROW(grid.mediumPerk(36, 0), std::out_of_range);
    EXPECT_THROW(grid.mediumPerk(0, -1), std::out_of_range);
    EXPECT_THROW(grid.largePerk(4, 0), std::out_of_range);
}

TEST(GridConfiguration, RevelationStages)
{
    const auto& grid = gridConfiguration();
    EXPECT_EQ(grid.revelationStage(-5), 0);
    EXPECT_EQ(grid.revelationStage(249), 0);
    EXPECT_EQ(grid.revelationStage(250), 1);
    EXPECT_EQ(grid.revelationStage(499), 1);
    EXPECT_EQ(grid.revelationStage(500), 2);
    EXPECT_EQ(grid.revelationStage(999), 2);
    EXPECT_EQ(grid.revelationStage(1000), 3);
    EXPECT_EQ(grid.revelationStage(5000), 3);
}

TEST(GridConfiguration, TileOrders)
{
    const auto& grid = gridConfiguration();
    const std::array<int, 4> centre = { 14, 15, 20, 21 };
    EXPECT_EQ(grid.centreTiles(), centre);
    const auto& order = grid.tilesFromCentre();
    EXPECT_EQ(order[0], 14);
    EXPECT_EQ(order[3], 20);
    EXPECT_EQ(order[4], 13);
    EXPECT_EQ(order[32], 0);
    EXPECT_EQ(order[35], 30);
    auto sorted = order;
    std::sort(sorted.begin(), sorted.end());
    for (int tile = 0; tile < 36; ++tile)
        EXPECT_EQ(sorted[tile], tile);
}
