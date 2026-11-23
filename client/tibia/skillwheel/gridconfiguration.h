#pragma once

#include <array>

namespace tibia {
namespace skillwheel {

/// The fixed layout of the skill wheel grid: the 36 tiles of the 6x6 grid (row by row from the
/// top left), each with its maximum points, its dedication (small) perk and its conviction
/// (medium) perk per vocation index (0 knight .. 4 monk); the revelation (large) perk of each of
/// the four domains; the revelation stage thresholds.
///
/// Out-of-range tiles, domains and vocation indices throw std::out_of_range.
class TGridConfiguration
{
public:
    virtual ~TGridConfiguration() = default;

    virtual int maxPoints(int tile) const;
    virtual int smallPerk(int tile, int vocationIndex) const;
    virtual int mediumPerk(int tile, int vocationIndex) const;
    virtual int largePerk(int domain, int vocationIndex) const;
    /// 0 below 250 points, 1 from 250, 2 from 500, 3 from 1000.
    virtual int revelationStage(int points) const;
    /// The four centre tiles every path starts from.
    virtual const std::array<int, 4>& centreTiles() const;
    /// All tiles ordered from the centre outwards, ring by ring.
    virtual const std::array<int, 36>& tilesFromCentre() const;
};

/// The grid configuration every skill wheel uses.
const TGridConfiguration& gridConfiguration();

}
}
