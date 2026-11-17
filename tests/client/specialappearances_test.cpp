#include "assets/specialappearances.h"

#include <gtest/gtest.h>

#include <stdexcept>

using tibia::appearances::TSpecialAppearancesForProtocolParsing;

TEST(SpecialAppearancesForProtocolParsing, AnswersMembershipPerKind)
{
    const TSpecialAppearancesForProtocolParsing special({ 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 }, { 8 }, { 9 });
    EXPECT_TRUE(special.isCumulative(1));
    EXPECT_TRUE(special.isLiquid(2));
    EXPECT_TRUE(special.isContainer(3));
    EXPECT_TRUE(special.isShowOffSocket(4));
    EXPECT_TRUE(special.isUpgradeable(5));
    EXPECT_TRUE(special.isWearOut(6));
    EXPECT_TRUE(special.isExpire(7));
    EXPECT_TRUE(special.isExpireStop(8));
    EXPECT_TRUE(special.isDecoKit(9));
    EXPECT_FALSE(special.isCumulative(2));
    EXPECT_FALSE(TSpecialAppearancesForProtocolParsing().isContainer(3));
}

TEST(SpecialAppearancesForProtocolParsing, RejectsAnIdInTwoKinds)
{
    EXPECT_THROW(TSpecialAppearancesForProtocolParsing({ 1 }, {}, { 1 }, {}, {}, {}, {}, {}, {}), std::invalid_argument);
    // Deco kits are not checked against the other kinds.
    EXPECT_NO_THROW(TSpecialAppearancesForProtocolParsing({ 1 }, {}, {}, {}, {}, {}, {}, {}, { 1 }));
}
