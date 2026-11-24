#include "protocol/unknowngameserverexception.h"

#include <gtest/gtest.h>

#include <string>

using tibia::protocol::TUnknownGameserverException;

TEST(UnknownGameserverException, NamesTheOpcodeInDecimal)
{
    const TUnknownGameserverException exception(0xd7);
    EXPECT_EQ(std::string(exception.what()), "Unknown Gameserver Message: 215");
    EXPECT_EQ(exception.opcode(), 0xd7);
}

TEST(UnknownGameserverException, IsAStdException)
{
    try {
        throw TUnknownGameserverException(7);
    } catch (const std::exception& exception) {
        EXPECT_EQ(std::string(exception.what()), "Unknown Gameserver Message: 7");
        return;
    }
    FAIL() << "not caught as std::exception";
}
