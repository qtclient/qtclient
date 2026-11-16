#include <gtest/gtest.h>

#include "shared/iodevicereader.h"
#include "shared/iodevicewriter.h"

#include <QBuffer>

#include <memory>
#include <stdexcept>

using tibia::shared::TIODeviceReader;
using tibia::shared::TIODeviceWriter;

namespace {

std::shared_ptr<QBuffer> bufferWith(const QByteArray& bytes)
{
    auto buffer = std::make_shared<QBuffer>();
    buffer->setData(bytes);
    buffer->open(QIODevice::ReadOnly);
    return buffer;
}

}

TEST(IODeviceWriter, WritesTypedValuesInTheByteOrder)
{
    auto buffer = std::make_shared<QBuffer>();
    {
        TIODeviceWriter writer(buffer, QDataStream::LittleEndian);
        writer.open(QIODevice::WriteOnly);
        writer.writeUInt8(0x01);
        writer.writeUInt16(0x0302);
        writer.writeUInt32(0x07060504);
        writer.writeBool(true);
        writer.writeString8(QStringLiteral("ab"));
        writer.writeString16(QStringLiteral("c"));
        EXPECT_EQ(writer.pos(), 14u);
    }
    // The writer opened the device, so it closed it.
    EXPECT_FALSE(buffer->isOpen());
    EXPECT_EQ(buffer->data(), QByteArray::fromHex("01020304050607010261620100" "63"));
}

TEST(IODeviceReader, ReadsTypedValuesInTheByteOrder)
{
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("01020304050607ff0261620100" "63")), QDataStream::LittleEndian);
    EXPECT_EQ(reader.peekUInt8(), 0x01);
    EXPECT_EQ(reader.readUInt8(), 0x01);
    EXPECT_EQ(reader.readUInt16(), 0x0302);
    EXPECT_EQ(reader.readUInt32(), 0x07060504u);
    EXPECT_EQ(reader.readInt8(), -1);
    EXPECT_EQ(reader.readString8(), QStringLiteral("ab"));
    EXPECT_EQ(reader.readString16(), QStringLiteral("c"));
    EXPECT_EQ(reader.pos(), 14u);
}

TEST(IODeviceReader, ReadsBigEndianAndSignedValues)
{
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("fffe" "fffffffe" "00000000000000ff")), QDataStream::BigEndian);
    EXPECT_EQ(reader.readInt16(), -2);
    EXPECT_EQ(reader.readInt32(), -2);
    EXPECT_EQ(reader.readInt64(), 0xff);
}

TEST(IODeviceReader, DoubleIsAPrecisionAndAnOffsetValue)
{
    // precision 2, value 0x7fffffff + 1234 -> 12.34
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("02" "d1040080")), QDataStream::LittleEndian);
    EXPECT_DOUBLE_EQ(reader.readDouble(), 12.34);
}

TEST(IODeviceReader, ThrowsWhenNotEnoughBytesAreAvailable)
{
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("0102")), QDataStream::LittleEndian);
    EXPECT_TRUE(reader.hasBytesAvailable(2));
    EXPECT_FALSE(reader.hasBytesAvailable(3));
    EXPECT_THROW(reader.readUInt32(), std::runtime_error);
    EXPECT_EQ(reader.readUInt16(), 0x0201);
}

TEST(IODeviceReader, SkipToReadsForwardOnly)
{
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("0102030405")), QDataStream::LittleEndian);
    reader.skipTo(3);
    EXPECT_EQ(reader.readUInt8(), 0x04);
    EXPECT_THROW(reader.skipTo(1), std::runtime_error);
    reader.seek(1);
    EXPECT_EQ(reader.readUInt8(), 0x02);
}

TEST(IODeviceReader, AtEndIsTheLastByte)
{
    TIODeviceReader reader(bufferWith(QByteArray::fromHex("010203")), QDataStream::LittleEndian);
    EXPECT_FALSE(reader.atEnd());
    reader.readUInt8();
    EXPECT_FALSE(reader.atEnd());
    reader.readUInt8();
    // CipSoft compares the position against size - 1.
    EXPECT_TRUE(reader.atEnd());
}

TEST(IODeviceReader, LeavesADeviceItDidNotOpenOpen)
{
    auto buffer = bufferWith(QByteArray("x"));
    {
        TIODeviceReader reader(buffer, QDataStream::LittleEndian);
        EXPECT_TRUE(reader.isOpen());
    }
    EXPECT_TRUE(buffer->isOpen());
}
