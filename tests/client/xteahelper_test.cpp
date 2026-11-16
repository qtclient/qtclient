#include <gtest/gtest.h>

#include "shared/xteahelper.h"

#include <algorithm>
#include <stdexcept>

using tibia::shared::TXteaHelper;

namespace {

const QByteArray Key = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");

// Swaps every 32-bit word: the same data in the other byte order.
QByteArray swapWords(const QByteArray& data)
{
    QByteArray swapped = data;
    for (qsizetype i = 0; i + 4 <= swapped.size(); i += 4)
        std::reverse(swapped.begin() + i, swapped.begin() + i + 4);
    return swapped;
}

}

TEST(XteaHelper, MatchesTheReferenceVectorInBigEndian)
{
    TXteaHelper xtea(QDataStream::BigEndian);
    xtea.setKey(Key);
    EXPECT_EQ(xtea.encrypt(QByteArray::fromHex("4142434445464748")), QByteArray::fromHex("497df3d072612cb5"));
    EXPECT_EQ(xtea.decrypt(QByteArray::fromHex("497df3d072612cb5")), QByteArray::fromHex("4142434445464748"));
}

TEST(XteaHelper, LittleEndianWordsAreConvertedAroundTheCipher)
{
    TXteaHelper xtea(QDataStream::LittleEndian);
    xtea.setKey(swapWords(Key));
    // The key is held as big-endian words.
    EXPECT_EQ(xtea.key(), Key);
    EXPECT_EQ(xtea.encrypt(swapWords(QByteArray::fromHex("4142434445464748"))),
              swapWords(QByteArray::fromHex("497df3d072612cb5")));
}

TEST(XteaHelper, EncryptPadsToWholeBlocks)
{
    TXteaHelper xtea(QDataStream::LittleEndian);
    xtea.setKey(Key);
    const QByteArray plain("hello world");
    const QByteArray padded = xtea.pad(plain);
    EXPECT_EQ(padded.size(), 16);
    EXPECT_TRUE(padded.startsWith(plain));
    const QByteArray cipher = xtea.encrypt(plain);
    ASSERT_EQ(cipher.size(), 16);
    EXPECT_TRUE(xtea.decrypt(cipher).startsWith(plain));
}

TEST(XteaHelper, RejectsABadKeyAndPartialBlocks)
{
    TXteaHelper xtea(QDataStream::LittleEndian);
    EXPECT_THROW(xtea.setKey(QByteArray(15, '\0')), std::invalid_argument);
    xtea.setKey(Key);
    EXPECT_THROW(xtea.decrypt(QByteArray(9, '\0')), std::invalid_argument);
}
