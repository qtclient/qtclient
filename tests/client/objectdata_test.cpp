#include "protocol/objectdata.h"

#include <gtest/gtest.h>

namespace {

using tibia::protocol::TObjectData;

TEST(TObjectData, HangableKeepsItsHookState)
{
    const TObjectData data = TObjectData::hangable(2034, 2);
    EXPECT_EQ(data.id, 2034);
    EXPECT_TRUE(data.isHangable());
    EXPECT_EQ(data.hookState, 2u);
    EXPECT_EQ(data.liquidType, 0u);
    EXPECT_EQ(data.count, 0);
    EXPECT_EQ(data.upgradeClassification, 0);
    EXPECT_EQ(data.decoItemId, 0);
}

TEST(TObjectData, EqualWhenEveryMemberIs)
{
    TObjectData a = TObjectData::hangable(100, 1);
    TObjectData b = TObjectData::hangable(100, 1);
    EXPECT_TRUE(a == b);
    b.hookState = 2;
    EXPECT_FALSE(a == b);
    b = a;
    b.decoItemId = 7;
    EXPECT_FALSE(a == b);
    b = a;
    b.upgradeClassification = 1;
    EXPECT_FALSE(a == b);
    // Same id, another kind.
    EXPECT_FALSE(TObjectData::plain(100) == TObjectData::hangable(100, 0));
}

TEST(TObjectData, OrderedByIdThenCountThenLiquidBeforeKind)
{
    EXPECT_TRUE(TObjectData::plain(1) < TObjectData::plain(2));
    // The count comes before the kind: a cumulative object of count 1 sorts after
    // a hangable one of the same id (count 0), although its kind is smaller.
    EXPECT_TRUE(TObjectData::hangable(5, 0) < TObjectData::cumulative(5, 1));
    // The liquid type comes before the kind too.
    EXPECT_TRUE(TObjectData::hangable(5, 9) < TObjectData::liquid(5, 1));
    // The upgrade classification comes before the kind.
    EXPECT_TRUE(TObjectData::hangable(5, 0) < TObjectData::upgradeable(5, 1));
    // With everything before it equal, the kind decides, then the hook state.
    EXPECT_TRUE(TObjectData::plain(5) < TObjectData::hangable(5, 0));
    EXPECT_TRUE(TObjectData::hangable(5, 1) < TObjectData::hangable(5, 2));
    // The liquid type compares signed.
    TObjectData negative = TObjectData::plain(5);
    negative.liquidType = 0x80000000u;
    EXPECT_TRUE(negative < TObjectData::plain(5));
    EXPECT_FALSE(TObjectData::plain(5) < TObjectData::plain(5));
}

}
