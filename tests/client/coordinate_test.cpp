#include <gtest/gtest.h>

#include "shared/coordinate.h"

#include <sstream>

using tibia::shared::TCoordinate;

TEST(Coordinate, ArithmeticIsComponentWise)
{
    const TCoordinate a(10, 20, 7);
    const TCoordinate b(3, -4, 1);
    EXPECT_EQ(a + b, TCoordinate(13, 16, 8));
    EXPECT_EQ(a - b, TCoordinate(7, 24, 6));
    EXPECT_EQ(TCoordinate::minimum(a, b), TCoordinate(3, -4, 1));
    EXPECT_EQ(TCoordinate::maximum(a, b), TCoordinate(10, 20, 7));
}

TEST(Coordinate, OrderIsXThenYThenZ)
{
    EXPECT_TRUE(TCoordinate(1, 9, 9) < TCoordinate(2, 0, 0));
    EXPECT_TRUE(TCoordinate(1, 1, 9) < TCoordinate(1, 2, 0));
    EXPECT_TRUE(TCoordinate(1, 1, 1) < TCoordinate(1, 1, 2));
    EXPECT_FALSE(TCoordinate(1, 1, 1) < TCoordinate(1, 1, 1));
    EXPECT_TRUE(TCoordinate(1, 1, 1) != TCoordinate(1, 1, 2));
    EXPECT_FALSE(TCoordinate(1, 1, 1) != TCoordinate(1, 1, 1));
}

TEST(Coordinate, PrintsAsBracketedTriple)
{
    std::ostringstream stream;
    stream << TCoordinate(32000, 31000, 7);
    EXPECT_EQ(stream.str(), "[32000,31000,7]");
    QString text;
    QDebug(&text) << TCoordinate(1, 2, 3);
    EXPECT_EQ(text.trimmed(), QStringLiteral("[1,2,3]"));
}
