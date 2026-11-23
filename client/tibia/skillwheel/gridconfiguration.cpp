#include "gridconfiguration.h"

#include <cstddef>

namespace tibia {
namespace skillwheel {

namespace {

constexpr int TileCount = 36;
constexpr int DomainCount = 4;
constexpr int VocationCount = 5;

/// One tile: its maximum points, then its small and medium perk per vocation index.
struct TTileConfiguration
{
    int maxPoints;
    std::array<int, VocationCount> smallPerk;
    std::array<int, VocationCount> mediumPerk;
};

constexpr std::array<TTileConfiguration, TileCount> Tiles = { {
    { 200, { 2, 2, 2, 2, 2 }, { 8, 17, 35, 28, 39 } },
    { 150, { 4, 4, 4, 4, 4 }, { 5, 5, 5, 5, 5 } },
    { 100, { 0, 0, 0, 0, 0 }, { 14, 21, 26, 32, 42 } },
    { 100, { 1, 1, 1, 1, 1 }, { 7, 15, 34, 34, 40 } },
    { 150, { 0, 0, 0, 0, 0 }, { 37, 37, 37, 37, 37 } },
    { 200, { 2, 2, 2, 2, 2 }, { 12, 22, 27, 29, 44 } },
    { 150, { 4, 4, 4, 4, 4 }, { 37, 37, 37, 37, 37 } },
    { 100, { 0, 0, 0, 0, 0 }, { 46, 46, 46, 46, 46 } },
    { 75, { 1, 1, 1, 1, 1 }, { 6, 6, 6, 6, 6 } },
    { 75, { 3, 3, 3, 3, 3 }, { 37, 37, 37, 37, 37 } },
    { 100, { 1, 1, 1, 1, 1 }, { 46, 46, 46, 46, 46 } },
    { 150, { 0, 0, 0, 0, 0 }, { 5, 5, 5, 5, 5 } },
    { 100, { 0, 0, 0, 0, 0 }, { 11, 19, 25, 30, 45 } },
    { 75, { 1, 1, 1, 1, 1 }, { 7, 15, 34, 34, 40 } },
    { 50, { 3, 3, 3, 3, 3 }, { 37, 37, 37, 37, 37 } },
    { 50, { 4, 4, 4, 4, 4 }, { 10, 18, 24, 31, 41 } },
    { 75, { 3, 3, 3, 3, 3 }, { 6, 6, 6, 6, 6 } },
    { 100, { 1, 1, 1, 1, 1 }, { 13, 20, 36, 33, 43 } },
    { 100, { 4, 4, 4, 4, 4 }, { 13, 20, 36, 33, 43 } },
    { 75, { 0, 0, 0, 0, 0 }, { 5, 5, 5, 5, 5 } },
    { 50, { 1, 1, 1, 1, 1 }, { 12, 22, 27, 29, 44 } },
    { 50, { 0, 0, 0, 0, 0 }, { 37, 37, 37, 37, 37 } },
    { 75, { 4, 4, 4, 4, 4 }, { 7, 15, 34, 34, 40 } },
    { 100, { 3, 3, 3, 3, 3 }, { 14, 21, 26, 32, 42 } },
    { 150, { 3, 3, 3, 3, 3 }, { 6, 6, 6, 6, 6 } },
    { 100, { 4, 4, 4, 4, 4 }, { 46, 46, 46, 46, 46 } },
    { 75, { 0, 0, 0, 0, 0 }, { 37, 37, 37, 37, 37 } },
    { 75, { 4, 4, 4, 4, 4 }, { 5, 5, 5, 5, 5 } },
    { 100, { 3, 3, 3, 3, 3 }, { 46, 46, 46, 46, 46 } },
    { 150, { 1, 1, 1, 1, 1 }, { 37, 37, 37, 37, 37 } },
    { 200, { 2, 2, 2, 2, 2 }, { 10, 18, 24, 31, 41 } },
    { 150, { 3, 3, 3, 3, 3 }, { 37, 37, 37, 37, 37 } },
    { 100, { 4, 4, 4, 4, 4 }, { 7, 15, 34, 34, 40 } },
    { 100, { 3, 3, 3, 3, 3 }, { 11, 19, 25, 30, 45 } },
    { 150, { 1, 1, 1, 1, 1 }, { 6, 6, 6, 6, 6 } },
    { 200, { 2, 2, 2, 2, 2 }, { 9, 16, 23, 35, 38 } },
} };

/// The revelation perk of each domain per vocation index.
constexpr std::array<std::array<int, VocationCount>, DomainCount> LargePerks = { {
    { 0, 0, 0, 0, 0 },
    { 1, 4, 7, 11, 13 },
    { 2, 5, 8, 10, 14 },
    { 3, 6, 9, 12, 15 },
} };

constexpr std::array<int, 4> CentreTiles = { 14, 15, 20, 21 };

constexpr std::array<int, TileCount> TilesFromCentre = {
    14, 15, 21, 20,
    13, 8, 9, 16, 22, 27, 26, 19,
    12, 7, 2, 3, 10, 17, 23, 28, 33, 32, 25, 18,
    6, 1, 4, 11, 29, 34, 31, 24,
    0, 5, 35, 30,
};

/// Unsigned like the indices of the tables: a negative index is out of range.
std::size_t index(const int value) { return static_cast<std::size_t>(static_cast<unsigned>(value)); }

}

int TGridConfiguration::maxPoints(const int tile) const { return Tiles.at(index(tile)).maxPoints; }

int TGridConfiguration::smallPerk(const int tile, const int vocationIndex) const
{
    return Tiles.at(index(tile)).smallPerk.at(index(vocationIndex));
}

int TGridConfiguration::mediumPerk(const int tile, const int vocationIndex) const
{
    return Tiles.at(index(tile)).mediumPerk.at(index(vocationIndex));
}

int TGridConfiguration::largePerk(const int domain, const int vocationIndex) const
{
    return LargePerks.at(index(domain)).at(index(vocationIndex));
}

int TGridConfiguration::revelationStage(const int points) const
{
    if (points < 250)
        return 0;
    if (points < 500)
        return 1;
    return points < 1000 ? 2 : 3;
}

const std::array<int, 4>& TGridConfiguration::centreTiles() const { return CentreTiles; }

const std::array<int, 36>& TGridConfiguration::tilesFromCentre() const { return TilesFromCentre; }

const TGridConfiguration& gridConfiguration()
{
    static const TGridConfiguration configuration;
    return configuration;
}

}
}
