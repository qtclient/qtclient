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
using tibia::skillwheel::SlotCount;

namespace {
int maxPoints(const int tile) { return tibia::skillwheel::gridConfiguration().maxPoints(tile); }
}

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

// CIP configurations keep a gem record per vessel; the export code carries their mods.
namespace {

using tibia::skillwheel::TGem;
using tibia::skillwheel::TWheel;

TGem gem(const int id, const int quality, const int basic1, const int basic2 = 0, const int supreme = 0)
{
    TGem g;
    g.id = id;
    g.quality = quality;
    g.basic1 = basic1;
    g.basic2 = basic2;
    g.supreme = supreme;
    return g;
}

/// The points of a wheel with `count` full Gem Vessel slices for a knight.
TPoints vesselSlices(const int count)
{
    TPoints points{};
    int filled = 0;
    for (int slot = 0; slot < SlotCount && filled < count; ++slot) {
        if (tibia::skillwheel::isVesselPerk(tibia::skillwheel::gridConfiguration().mediumPerk(slot, 0))) {
            points[slot] = maxPoints(slot);
            ++filled;
        }
    }
    return points;
}

}

TEST(SkillWheelVessels, SocketChecksTheQualityAndMovesTheGem)
{
    TWheel wheel;
    // Vessel 0 holds lesser gems, vessel 1 regular ones, vessel 3 lesser ones.
    EXPECT_FALSE(tibia::skillwheel::socketGem(wheel, gem(7, 1, 2, 3), 0));
    EXPECT_FALSE(wheel.vessels[0].has_value());
    EXPECT_TRUE(tibia::skillwheel::socketGem(wheel, gem(7, 0, 2), 0));
    EXPECT_TRUE(tibia::skillwheel::socketGem(wheel, gem(7, 0, 2), 3));
    EXPECT_FALSE(wheel.vessels[0].has_value());
    ASSERT_TRUE(wheel.vessels[3].has_value());
    EXPECT_EQ(tibia::skillwheel::vesselOfGem(wheel, 7), 3);
}

TEST(SkillWheelVessels, ExportCodeKeepsTheVesselMods)
{
    TWheel wheel;
    wheel.vocation = 1;
    wheel.points[14] = 1;
    tibia::skillwheel::socketGem(wheel, gem(40, 0, 5), 0);
    tibia::skillwheel::socketGem(wheel, gem(41, 2, 6, 7, 0x70), 2);
    const QString code = tibia::skillwheel::encodeWheelCode(wheel);
    ASSERT_TRUE(code.startsWith(QStringLiteral("K1")));
    const std::optional<TWheel> decoded = tibia::skillwheel::decodeWheelCode(code);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->vocation, 1);
    EXPECT_EQ(decoded->points, wheel.points);
    // A decoded gem's id is its vessel index; the domain is not in the code.
    ASSERT_TRUE(decoded->vessels[0].has_value());
    EXPECT_EQ(decoded->vessels[0]->id, 0);
    EXPECT_EQ(decoded->vessels[0]->basic1, 5);
    ASSERT_TRUE(decoded->vessels[2].has_value());
    EXPECT_EQ(decoded->vessels[2]->id, 2);
    EXPECT_EQ(decoded->vessels[2]->quality, 2);
    EXPECT_EQ(decoded->vessels[2]->supreme, 0x70);
    EXPECT_FALSE(decoded->vessels[1].has_value());

    // Version 0 carries no gems.
    QString withoutGems = code;
    withoutGems[1] = QLatin1Char('0');
    const std::optional<TWheel> plain = tibia::skillwheel::decodeWheelCode(withoutGems);
    ASSERT_TRUE(plain.has_value());
    EXPECT_FALSE(plain->vessels[0].has_value());
    EXPECT_FALSE(plain->vessels[2].has_value());
}

TEST(SkillWheelVessels, ActiveModsCountUnlockedVesselsOnly)
{
    TWheel wheel;
    wheel.vocation = 1;
    tibia::skillwheel::socketGem(wheel, gem(1, 0, 3), 0); // basic mod 3: stat 3, +2 at grade 0
    tibia::skillwheel::TGrades none;

    // No Gem Vessel slice: the stat is listed nowhere because its sum is 0.
    EXPECT_TRUE(tibia::skillwheel::activeMods(wheel, none, none).basicStats.empty());

    wheel.points = vesselSlices(1);
    const tibia::skillwheel::TActiveMods mods = tibia::skillwheel::activeMods(wheel, none, none);
    ASSERT_EQ(mods.basicStats.size(), 1u);
    EXPECT_EQ(mods.basicStats[0].first, 3);
    EXPECT_DOUBLE_EQ(mods.basicStats[0].second, 2.0);
    EXPECT_EQ(mods.damageHealing, 1);
}
