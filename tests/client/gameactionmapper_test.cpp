#include <gtest/gtest.h>

#include "input/gameactionmapper.h"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <vector>

using tibia::input::EGameAction;
using tibia::input::TGameActionList;
using tibia::input::TGameActionMapper;
using A = EGameAction;

namespace tibia::input {

// gameactionmapper.cpp calls filterActions from map(); stub so the test binary
// does not need the full gameaction.cpp TU. map() is exercised via ruleTypes
// coverage below (Classic right-click type order is what selects OPEN).
TGameActionList filterActions(const TGameActionList& /*candidates*/, const std::span<const EGameAction> /*types*/)
{
    return {};
}

}

namespace {

bool hasType(const std::vector<A>& types, const A type)
{
    return std::find(types.begin(), types.end(), type) != types.end();
}

}

constexpr int Classic = TGameActionMapper::ClassicControlScheme;
constexpr int Legacy = TGameActionMapper::LegacyControlScheme;
constexpr int Smart = TGameActionMapper::LeftSmartClickControlScheme;

// Classic + Loot: Right: right click puts quick loot right after use; the
// shift+right quick loot rule is gone; left click only walks.
TEST(GameActionMapper, ClassicLootRightWorldMapRuleTypes)
{
    const TGameActionMapper mapper(Classic, TGameActionMapper::LootRight);

    const std::vector<A> left = mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left.front(), A::SET_PLAYER_MOVEMENT_INTENT);

    const std::vector<A> right = mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, 0);
    const std::vector<A> expected = { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC, A::USE, A::QUICK_LOOT,
                                      A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
                                      A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE };
    EXPECT_EQ(right, expected);

    EXPECT_TRUE(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::ShiftModifier).empty());
}

// Classic + Loot: Right: containers get a right click without quick loot; the
// left click keeps the classic container quick loot.
TEST(GameActionMapper, ClassicLootRightContainerContext)
{
    const TGameActionMapper mapper(Classic, TGameActionMapper::LootRight);

    const std::vector<A> right = mapper.ruleTypes(TGameActionMapper::ContainerContext, Qt::RightButton, 0);
    ASSERT_FALSE(right.empty());
    EXPECT_EQ(right.front(), A::ATTACK_CREATURE);
    EXPECT_TRUE(hasType(right, A::OPEN));
    EXPECT_FALSE(hasType(right, A::QUICK_LOOT));

    const std::vector<A> left = mapper.ruleTypes(TGameActionMapper::ContainerContext, Qt::LeftButton, 0);
    EXPECT_EQ(left, std::vector<A>{ A::QUICK_LOOT });
}

TEST(GameActionMapper, ClassicLootShiftRightKeepsTheTable)
{
    const TGameActionMapper mapper(Classic, TGameActionMapper::LootShiftRight);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::ShiftModifier),
              std::vector<A>{ A::QUICK_LOOT });
    EXPECT_FALSE(hasType(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, 0), A::QUICK_LOOT));
}

TEST(GameActionMapper, ClassicLootLeftPutsQuickLootFirstOnLeftClick)
{
    const TGameActionMapper mapper(Classic, TGameActionMapper::LootLeft);
    const std::vector<A> left = mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0);
    EXPECT_EQ(left, (std::vector<A>{ A::QUICK_LOOT, A::SET_PLAYER_MOVEMENT_INTENT }));
    EXPECT_TRUE(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::ShiftModifier).empty());
}

// The loot scheme rewrites only the classic rules.
TEST(GameActionMapper, LegacyIgnoresTheLootScheme)
{
    const TGameActionMapper mapper(Legacy, TGameActionMapper::LootRight);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::ShiftModifier),
              std::vector<A>{ A::QUICK_LOOT });
}

// A new control scheme drops the loot rewrite.
TEST(GameActionMapper, SetControlSchemeResetsTheRules)
{
    TGameActionMapper mapper(Classic, TGameActionMapper::LootLeft);
    mapper.setControlScheme(Classic);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0),
              std::vector<A>{ A::SET_PLAYER_MOVEMENT_INTENT });
    EXPECT_EQ(mapper.controlScheme(), Classic);
    EXPECT_THROW(mapper.setControlScheme(0), std::out_of_range);
}

// A scheme that has a table keeps it; a new scheme's table is used once it is
// selected; while rules are active, adding a scheme does not switch to it.
TEST(GameActionMapper, AddControlSchemeKeepsExistingTables)
{
    using Rule = TGameActionMapper::TRule;
    TGameActionMapper mapper(Classic, TGameActionMapper::LootShiftRight);
    mapper.addControlScheme(Classic, { Rule{ 0, 1, 0, { A::LOOK } } });
    EXPECT_EQ(mapper.controlScheme(), Classic);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0),
              std::vector<A>{ A::SET_PLAYER_MOVEMENT_INTENT });

    mapper.addControlScheme(7, { Rule{ 0, 1, 0, { A::LOOK } } });
    EXPECT_EQ(mapper.controlScheme(), Classic);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0),
              std::vector<A>{ A::SET_PLAYER_MOVEMENT_INTENT });

    mapper.setControlScheme(7);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0), std::vector<A>{ A::LOOK });
    mapper.setControlScheme(Classic);
    EXPECT_EQ(mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0),
              std::vector<A>{ A::SET_PLAYER_MOVEMENT_INTENT });
}

TEST(GameActionMapper, SmartLeftClickIsNotClassic)
{
    const TGameActionMapper smart(Smart, TGameActionMapper::LootRight);
    const std::vector<A> left = smart.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left.front(), A::SMART_CLICK);
}

// CipSoft Classic: context menu is Ctrl+click (Cmd on macOS → MetaModifier).
TEST(GameActionMapper, ClassicMenuRequiresCtrlOrMeta)
{
    const TGameActionMapper mapper(Classic, TGameActionMapper::LootShiftRight);

    const std::vector<A> plainRight =
        mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, 0);
    EXPECT_FALSE(hasType(plainRight, A::CONTEXT_MENU));
    EXPECT_TRUE(hasType(plainRight, A::USE) || hasType(plainRight, A::OPEN));

    const std::vector<A> ctrlRight =
        mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::ControlModifier);
    EXPECT_TRUE(hasType(ctrlRight, A::CONTEXT_MENU));

    const std::vector<A> metaRight =
        mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, Qt::MetaModifier);
    EXPECT_TRUE(hasType(metaRight, A::CONTEXT_MENU));
}
