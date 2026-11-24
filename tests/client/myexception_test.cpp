#include "input/myexception.h"

#include <gtest/gtest.h>

TEST(MyException, LocationIsFileNameAndLine)
{
    const TMyException e("/builds/src/input/Container.h", 133, "index out of range");
    EXPECT_EQ(e.location(), "Container.h 133");
    EXPECT_STREQ(e.what(), "Container.h 133: index out of range");
}

TEST(MyException, BackslashSeparatesToo)
{
    const TMyException e("C:\\src\\input\\Container.h", 7, "m");
    EXPECT_EQ(e.location(), "Container.h 7");
}

TEST(MyException, PathWithoutFileNameStaysWhole)
{
    EXPECT_EQ(TMyException("Container.h", 1, "m").location(), "Container.h 1");
    EXPECT_EQ(TMyException("src/", 1, "m").location(), "src/ 1");
}

TEST(MyException, NamedValuesAndReason)
{
    const TMyException e("a/b.h", 5, "bad", "x", 3, "y", -1, "why");
    EXPECT_STREQ(e.what(), "b.h 5: bad (x = 3) (y = -1), reason:\nwhy");
}

TEST(MyException, EmptyNamesAreLeftOut)
{
    const TMyException e("b.h", 5, "bad", "", 3, "y", 4);
    EXPECT_STREQ(e.what(), "b.h 5: bad (y = 4)");
}
