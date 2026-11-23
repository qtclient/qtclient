#include <gtest/gtest.h>

#include "creatures/movementdirection.h"

#include <sstream>
#include <stdexcept>

using tibia::creatures::ECreatureLookDirection;
using tibia::creatures::EMovementDirection;

TEST(MovementDirectionNames, StepsAndLookConversion)
{
    EXPECT_TRUE(isStep(EMovementDirection::NorthWest));
    EXPECT_FALSE(isStep(EMovementDirection::None));
    EXPECT_EQ(movementDirectionOfLook(ECreatureLookDirection::North), EMovementDirection::North);
    EXPECT_EQ(movementDirectionOfLook(ECreatureLookDirection::East), EMovementDirection::East);
    EXPECT_EQ(movementDirectionOfLook(ECreatureLookDirection::West), EMovementDirection::West);
    EXPECT_THROW(movementDirectionOfLook(static_cast<ECreatureLookDirection>(4)), std::logic_error);
}

TEST(MovementDirectionNames, StreamNames)
{
    std::ostringstream s;
    s << EMovementDirection::SouthWest << ',' << EMovementDirection::None << ',' << EMovementDirection::TurnWest << ','
      << ECreatureLookDirection::South << ',' << static_cast<EMovementDirection>(13) << '.';
    EXPECT_EQ(s.str(), "SOUTHWEST,TELEPORT_UP_OR_DOWN,TELEPORT_WEST,SOUTH,.");
}
