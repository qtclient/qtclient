// CIP skill wheel grid rules (TSkillWheelPageController's grid path solver): every tile with points
// is a centre tile or reaches one through full tiles; no tile is above its maximum.

#include "skillwheel/skillwheeldata.h"
#include "spells/spellinfostorage.h"

#include <gtest/gtest.h>

// skillwheeldata.cpp names spells for augmentation texts; the grid rules never ask.
namespace tibia::spells {
TSpellInfoStorage* TSpellInfoStorage::instance() { return nullptr; }
const TSpellInfo* TSpellInfoStorage::spell(int) const { return nullptr; }
}

using tibia::skillwheel::canDecrease;
using tibia::skillwheel::isUnlocked;
using tibia::skillwheel::isValid;
using tibia::skillwheel::maxPoints;
using tibia::skillwheel::SlotCount;

namespace {

using TPoints = std::array<int, SlotCount>;

TPoints full(std::initializer_list<int> tiles)
{
    TPoints points{};
    for (const int tile : tiles)
        points[tile] = maxPoints(tile);
    return points;
}

}

TEST(SkillWheelGrid, EmptyGridAndCentreTilesAreValid)
{
    EXPECT_TRUE(isValid(TPoints{}));
    TPoints points{};
    points[14] = 1;
    points[21] = 1;
    EXPECT_TRUE(isValid(points));
}

TEST(SkillWheelGrid, TileAboveItsMaximumIsInvalid)
{
    TPoints points{};
    points[14] = maxPoints(14) + 1;
    EXPECT_FALSE(isValid(points));
}

TEST(SkillWheelGrid, PathsNeedFullTiles)
{
    TPoints points{};
    points[14] = maxPoints(14) - 1;
    points[13] = 1;
    EXPECT_FALSE(isValid(points)); // 13 reaches the centre only through a tile that is not full
    points[14] = maxPoints(14);
    EXPECT_TRUE(isValid(points));
    points[0] = 1; // no full path from the corner
    EXPECT_FALSE(isValid(points));
}

TEST(SkillWheelGrid, LinksAcrossDomainBorders)
{
    // 3 touches 2, 4 and 9 on the grid (all empty); only the border link 3-10 reaches the full 10.
    TPoints points = full({ 15, 16, 10 });
    points[3] = 5;
    EXPECT_TRUE(isValid(points));
    points[10] = maxPoints(10) - 1;
    EXPECT_FALSE(isValid(points));
}

TEST(SkillWheelGrid, DecreaseAndUnlock)
{
    TPoints points = full({ 14 });
    points[13] = 1;
    EXPECT_FALSE(canDecrease(points, 14)); // 13 hangs on the full 14
    EXPECT_TRUE(canDecrease(points, 13));
    EXPECT_TRUE(isUnlocked(points, 8));  // next to the full 14
    EXPECT_FALSE(isUnlocked(points, 0)); // nothing full next to it
}
