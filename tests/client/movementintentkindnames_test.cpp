#include <gtest/gtest.h>

#include "input/playermovementintent.h"

#include <sstream>

using namespace tibia::input;

namespace {

std::string nameOf(const TMovementIntentKind kind)
{
    std::ostringstream stream;
    stream << kind;
    return stream.str();
}

}

TEST(MovementIntentKindNames, EachKindWritesItsCipSoftName)
{
    EXPECT_EQ(nameOf(TMovementIntentKind::Target), "TARGET_COORDINATE");
    EXPECT_EQ(nameOf(TMovementIntentKind::Direction), "DIRECTION");
    EXPECT_EQ(nameOf(TMovementIntentKind::Rotate), "ROTATION");
    EXPECT_EQ(nameOf(TMovementIntentKind::Stop), "NONE");
}

TEST(MovementIntentKindNames, AnUnknownKindWritesNothing)
{
    EXPECT_EQ(nameOf(static_cast<TMovementIntentKind>(4)), "");
    EXPECT_EQ(nameOf(static_cast<TMovementIntentKind>(-1)), "");
}
