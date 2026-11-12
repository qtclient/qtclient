#include <gtest/gtest.h>

#include "input/gameactionmapper.h"

#include <algorithm>
#include <span>
#include <vector>

using tibia::input::EGameAction;
using tibia::input::TGameActionList;
using tibia::input::TGameActionMapper;
using tibia::input::TGameActionPtr;
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

// CIP Classic rules (0x1077b2668) + lootSchemeIndex 0 (Loot: Right):
// right-click use/open with QUICK_LOOT injected first; left-click only walks.
TEST(GameActionMapper, ClassicLootRightWorldMapRuleTypes)
{
    const TGameActionMapper mapper(/*controlScheme=*/0, /*lootScheme=*/0);

    const std::vector<A> left = mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left.front(), A::SET_PLAYER_MOVEMENT_INTENT);

    const std::vector<A> right = mapper.ruleTypes(TGameActionMapper::WorldMapContext, Qt::RightButton, 0);
    ASSERT_FALSE(right.empty());
    EXPECT_EQ(right.front(), A::QUICK_LOOT);
    EXPECT_TRUE(hasType(right, A::OPEN));
    EXPECT_TRUE(hasType(right, A::OPEN_IN_NEW_WINDOW));
    EXPECT_TRUE(hasType(right, A::USE));
    EXPECT_TRUE(hasType(right, A::USE_WITH));
    EXPECT_FALSE(hasType(right, A::SET_PLAYER_MOVEMENT_INTENT));
    EXPECT_FALSE(hasType(right, A::SMART_CLICK));
}

TEST(GameActionMapper, ClassicLootRightContainerContext)
{
    const TGameActionMapper mapper(/*controlScheme=*/0, /*lootScheme=*/0);

    // Left is not the loot chord → Classic container left QUICK_LOOT is stripped.
    const std::vector<A> left = mapper.ruleTypes(TGameActionMapper::ContainerContext, Qt::LeftButton, 0);
    EXPECT_TRUE(left.empty());

    // Right falls through to world-map use/open and injects QUICK_LOOT.
    const std::vector<A> right = mapper.ruleTypes(TGameActionMapper::ContainerContext, Qt::RightButton, 0);
    ASSERT_FALSE(right.empty());
    EXPECT_EQ(right.front(), A::QUICK_LOOT);
    EXPECT_TRUE(hasType(right, A::OPEN));
}

TEST(GameActionMapper, SmartLeftClickIsNotClassic)
{
    const TGameActionMapper smart(/*controlScheme=*/2, /*lootScheme=*/0);
    const std::vector<A> left = smart.ruleTypes(TGameActionMapper::WorldMapContext, Qt::LeftButton, 0);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left.front(), A::SMART_CLICK);
}

// CipSoft Classic: context menu is Ctrl+click (Cmd on macOS → MetaModifier).
TEST(GameActionMapper, ClassicMenuRequiresCtrlOrMeta)
{
    const TGameActionMapper mapper(/*controlScheme=*/0, /*lootScheme=*/1); // shift+right loot, not plain right

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
