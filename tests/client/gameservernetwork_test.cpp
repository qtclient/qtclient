// CIP network:: packet layer: TGameserverNetworkPacketReader,
// TGameserverNetworkPacketWriter, TUnencryptedRawMessageStream and
// TGameserverNetworkPacketSequenceFlowProcessor.

#include "network/gameservernetworkpacket.h"
#include "network/gameservernetworkpacketreader.h"
#include "network/gameservernetworkpacketsequenceflowprocessor.h"
#include "network/gameservernetworkpacketwriter.h"
#include "network/unencryptedrawmessagestream.h"
#include "shared/iodevicereader.h"
#include "shared/iodevicewriter.h"

#include <QBuffer>
#include <QDataStream>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using tibia::network::TGameserverNetworkPacket;
using tibia::network::TGameserverNetworkPacketReader;
using tibia::network::TGameserverNetworkPacketSequenceFlowProcessor;
using tibia::network::TGameserverNetworkPacketWriter;
using tibia::network::TUnencryptedRawMessageStream;

namespace {

QByteArray header(uint16_t blocks, uint32_t word)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << static_cast<qint16>(blocks) << static_cast<qint32>(word);
    return bytes;
}

std::unique_ptr<TGameserverNetworkPacketReader> readerOver(const std::shared_ptr<QBuffer>& buffer)
{
    buffer->open(QIODevice::ReadOnly);
    auto deviceReader = std::make_shared<tibia::shared::TIODeviceReader>(buffer, QDataStream::LittleEndian);
    return std::make_unique<TGameserverNetworkPacketReader>(deviceReader);
}

TGameserverNetworkPacket xteaPacket(uint32_t sequenceNumber, int connectionIndex = 0)
{
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameserver, QByteArray(1, char(sequenceNumber)));
    packet.setSequenceNumber(sequenceNumber);
    packet.setEncryptionType(TGameserverNetworkPacket::Xtea);
    packet.setConnectionIndex(connectionIndex);
    return packet;
}

}

TEST(GameserverNetworkPacketReader, ReadsHeaderAndPayload)
{
    auto buffer = std::make_shared<QBuffer>();
    buffer->setData(header(1, 0x80000005u) + QByteArray("abcdefgh"));
    auto reader = readerOver(buffer);

    EXPECT_FALSE(reader->isHeaderRead());
    reader->readHeader();
    ASSERT_TRUE(reader->isHeaderRead());
    EXPECT_EQ(reader->header().payloadSize, 8u);
    EXPECT_EQ(reader->header().packetSize, 14u);
    ASSERT_TRUE(reader->isPacketComplete());

    const TGameserverNetworkPacket packet = reader->takePacket();
    EXPECT_EQ(packet.sequenceNumber(), 5u);
    EXPECT_EQ(packet.compressionType(), TGameserverNetworkPacket::OneShot);
    EXPECT_EQ(packet.data(), QByteArray("abcdefgh"));
    EXPECT_FALSE(reader->isHeaderRead());
}

TEST(GameserverNetworkPacketReader, Bit30IsStreamingCompressionAndSequenceHasThirtyBits)
{
    auto buffer = std::make_shared<QBuffer>();
    buffer->setData(header(1, 0xC0000007u) + QByteArray(8, 'x'));
    auto reader = readerOver(buffer);
    reader->readHeader();
    const TGameserverNetworkPacket packet = reader->takePacket();
    EXPECT_EQ(packet.sequenceNumber(), 7u);
    EXPECT_EQ(packet.compressionType(), TGameserverNetworkPacket::Streaming);
}

TEST(GameserverNetworkPacketReader, WaitsForTheWholeHeaderAndPayload)
{
    auto buffer = std::make_shared<QBuffer>();
    buffer->setData(header(1, 1).left(4));
    auto reader = readerOver(buffer);
    reader->readHeader();
    EXPECT_FALSE(reader->isHeaderRead());
    EXPECT_FALSE(reader->isPacketComplete());
    EXPECT_THROW(reader->takePacket(), std::runtime_error);

    auto partial = std::make_shared<QBuffer>();
    partial->setData(header(2, 1) + QByteArray(8, 'x'));
    auto partialReader = readerOver(partial);
    partialReader->readHeader();
    EXPECT_TRUE(partialReader->isHeaderRead());
    EXPECT_FALSE(partialReader->isPacketComplete());
}

TEST(GameserverNetworkPacketWriter, WritesBlockCountSequenceAndPayload)
{
    auto buffer = std::make_shared<QBuffer>();
    buffer->open(QIODevice::WriteOnly);
    TGameserverNetworkPacketWriter writer(std::make_shared<tibia::shared::TIODeviceWriter>(buffer, QDataStream::LittleEndian));
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, QByteArray(9, 'p'));
    packet.setSequenceNumber(0x01020304);
    writer.writePacket(packet);
    EXPECT_EQ(buffer->data(), header(2, 0x01020304) + QByteArray(9, 'p'));
}

TEST(UnencryptedRawMessageStream, AppendKeepsReadPositionAndRemoveDropsReadBytes)
{
    TUnencryptedRawMessageStream stream;
    stream.appendData("abc");
    EXPECT_EQ(stream.pos(), 0);
    EXPECT_EQ(stream.read(2), QByteArray("ab"));
    stream.appendData("de");
    EXPECT_EQ(stream.pos(), 2);
    stream.removeReadData();
    EXPECT_EQ(stream.pos(), 0);
    EXPECT_EQ(stream.buffer(), QByteArray("cde"));
    EXPECT_EQ(stream.readAllData(), QByteArray("cde"));
}

TEST(GameserverNetworkPacketSequenceFlowProcessor, PassesOnInSequenceOrderAndDropsCopies)
{
    TGameserverNetworkPacketSequenceFlowProcessor processor;
    std::vector<uint32_t> passedOn;
    QObject::connect(&processor, &TGameserverNetworkPacketSequenceFlowProcessor::packetReady,
                     [&passedOn](const TGameserverNetworkPacket& packet) { passedOn.push_back(packet.sequenceNumber()); });

    // Before the login: as it comes, whatever the sequence word says.
    TGameserverNetworkPacket challenge(TGameserverNetworkPacket::Gameserver, QByteArray("c"));
    challenge.setSequenceNumber(0x2d9c0383);
    processor.processPacket(challenge);
    ASSERT_EQ(passedOn.size(), 1u);

    processor.processPacket(xteaPacket(5)); // the first XTEA packet starts the sequence
    processor.processPacket(xteaPacket(7)); // gap: waits for 6
    EXPECT_EQ(passedOn, (std::vector<uint32_t>{ 0x2d9c0383, 5 }));
    processor.processPacket(xteaPacket(6, 1));
    EXPECT_EQ(passedOn, (std::vector<uint32_t>{ 0x2d9c0383, 5, 6, 7 }));
    processor.processPacket(xteaPacket(6, 1)); // the other connection's copy
    processor.processPacket(xteaPacket(5));
    EXPECT_EQ(passedOn, (std::vector<uint32_t>{ 0x2d9c0383, 5, 6, 7 }));
    processor.processPacket(xteaPacket(8));
    EXPECT_EQ(passedOn.back(), 8u);
}
