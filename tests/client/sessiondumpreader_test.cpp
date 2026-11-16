#include <gtest/gtest.h>

#include "sessiondump/lightweightsessiondumpmessageprocessor.h"
#include "sessiondump/sessiondumpfactory.h"
#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumpmetadatareader.h"
#include "sessiondump/sessiondumppacketprocessor.h"
#include "sessiondump/sessiondumppacketreader.h"
#include "sessiondump/sessiondumpprocessor.h"
#include "sessiondump/sessiondumpreader.h"
#include "shared/iodevicereader.h"

#include <QBuffer>
#include <QFile>

#include <stdexcept>
#include <vector>

using namespace tibia::sessiondump;
using tibia::shared::TIODeviceReader;

namespace {

void appendU16(QByteArray& out, const uint16_t v)
{
    out.append(char(v));
    out.append(char(v >> 8));
}

void appendU32(QByteArray& out, const uint32_t v)
{
    appendU16(out, uint16_t(v));
    appendU16(out, uint16_t(v >> 16));
}

void appendString8(QByteArray& out, const QByteArray& s)
{
    out.append(char(s.size()));
    out.append(s);
}

QByteArray message(const char magic, const QByteArray& body)
{
    QByteArray out(1, magic);
    appendU16(out, uint16_t(body.size()));
    return out + body;
}

QByteArray packet(const uint8_t type, const uint32_t timestamp, const QByteArray& messages)
{
    QByteArray out(1, 'p');
    appendU32(out, uint32_t(1 + 4 + messages.size()));
    out.append(char(type));
    appendU32(out, timestamp);
    return out + messages;
}

/// A version 7 dump with two packets.
QByteArray dump()
{
    QByteArray metadata;
    appendU16(metadata, 7);
    appendU32(metadata, 1521);
    metadata.append(char(0)); // recording mode
    appendString8(metadata, "KEY");
    appendU32(metadata, 1758867836);
    appendString8(metadata, "Knight");
    appendU32(metadata, 1002);
    appendString8(metadata, "Antica");
    metadata.append(char(0));
    appendString8(metadata, "");
    appendU16(metadata, 1);
    appendU16(metadata, 894);
    QByteArray out("dmp");
    appendU16(out, uint16_t(metadata.size()));
    out += metadata;
    out += packet(1, 0, message('m', "\x08\x01" "ab") + message('c', "xy"));
    out += packet(2, 51, message('m', "\x08\x17"));
    return out;
}

struct TDump
{
    std::shared_ptr<TIODeviceReader> io;
    std::shared_ptr<TSessiondumpFactory> factory = std::make_shared<TSessiondumpFactory>();

    explicit TDump(const QByteArray& bytes)
    {
        auto buffer = std::make_shared<QBuffer>();
        buffer->setData(bytes);
        io = std::make_shared<TIODeviceReader>(buffer, QDataStream::LittleEndian);
    }
};

}

TEST(SessiondumpReader, ReadsTheMetadata)
{
    TDump d(dump());
    const auto reader = d.factory->createReader(d.io);
    EXPECT_EQ(reader->state(), TSessiondumpReader::Closed);
    const TSessiondumpMetadata metadata = reader->metadata();
    EXPECT_EQ(reader->state(), TSessiondumpReader::MetadataRead);
    EXPECT_TRUE(metadata.read);
    EXPECT_EQ(metadata.version, 7);
    EXPECT_EQ(metadata.clientVersion, 1521u);
    EXPECT_EQ(metadata.sessionKey, QStringLiteral("KEY"));
    EXPECT_EQ(metadata.creationTime.toSecsSinceEpoch(), 1758867836);
    EXPECT_EQ(metadata.characterName, QStringLiteral("Knight"));
    EXPECT_EQ(metadata.worldId, 1002u);
    EXPECT_EQ(metadata.worldName, QStringLiteral("Antica"));
    EXPECT_EQ(metadata.extraValue2, 894);
    EXPECT_EQ(reader->metadataReader()->metadataEnd(), uint64_t(metadata.size) + 5);
    EXPECT_EQ(d.io->pos(), reader->metadataReader()->metadataEnd());
}

TEST(SessiondumpReader, BadMagicThrows)
{
    TDump d(QByteArray("dmq\x02\x00\x07\x00", 7));
    const auto reader = d.factory->createReader(d.io);
    EXPECT_THROW(reader->metadata(), std::runtime_error);
}

TEST(SessiondumpReader, PacketAndMessageHeaders)
{
    TDump d(dump());
    const auto reader = d.factory->createReader(d.io);
    reader->metadata();
    const auto packets = reader->packetReader();
    packets->readHeader();
    ASSERT_TRUE(packets->isHeaderRead());
    const TSessiondumpPacketInfo packet = packets->info();
    EXPECT_EQ(packet.type, 1u);
    EXPECT_EQ(packet.timestamp, 0u);
    EXPECT_EQ(packet.payloadSize, 7u + 5u);
    EXPECT_TRUE(packets->hasCompletePayload());

    const auto messages = reader->messageReader();
    messages->readHeader();
    const TSessiondumpMessageInfo message = messages->info();
    EXPECT_TRUE(message.isServerMessage());
    EXPECT_EQ(message.length, 4);
    EXPECT_EQ(message.firstByte, 0x08);
    EXPECT_EQ(messages->body(), QByteArray("\x08\x01" "ab"));
    EXPECT_TRUE(messages->atStart());
    messages->skipMessage();
    EXPECT_FALSE(messages->isHeaderRead());
    messages->readHeader();
    EXPECT_FALSE(messages->info().isServerMessage());
    messages->skipMessage();
    EXPECT_TRUE(packets->atEnd());
    packets->skipPacket();
    EXPECT_FALSE(packets->isHeaderRead());
}

TEST(SessiondumpReader, ProcessorHandsEveryMessageWithItsPacket)
{
    TDump d(dump());
    std::vector<std::pair<uint64_t, QByteArray>> seen;
    const auto messageProcessor = std::make_shared<TLightweightSessiondumpMessageProcessor>(
        d.factory->createMessageReader(d.io),
        [&seen](const TSessiondumpPacketInfo& packet, const TSessiondumpMessageInfo& info, const std::function<QByteArray()>& body) {
            seen.emplace_back(packet.timestamp, QByteArray(1, char(info.magic)) + body());
        });
    TSessiondumpProcessor processor(d.factory->createReader(d.io),
                                    d.factory->createPacketProcessor(d.factory->createPacketReader(d.io), messageProcessor));
    processor.start();
    processor.processUntil(10);
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], std::make_pair(uint64_t(0), QByteArray("m\x08\x01" "ab")));
    EXPECT_EQ(seen[1], std::make_pair(uint64_t(0), QByteArray("cxy")));
    EXPECT_TRUE(processor.hasNextPacket());
    EXPECT_EQ(processor.nextPacketInfo().timestamp, 51u);
    processor.processUntil(51);
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[2].first, 51u);
    EXPECT_FALSE(processor.hasNextPacket());
    EXPECT_EQ(processor.currentPacketInfo().type, 2u);
}

TEST(SessiondumpReader, ReadsTheTutorialDump)
{
    auto file = std::make_shared<QFile>(QStringLiteral(QTCLIENT_TESTDATA_DIR "/sessiondumps/tutorial-sessiondump.dmp"));
    if (!file->exists())
        GTEST_SKIP() << "no tutorial dump";
    const auto io = std::make_shared<TIODeviceReader>(file, QDataStream::LittleEndian);
    const auto factory = std::make_shared<TSessiondumpFactory>();
    int messages = 0;
    uint64_t lastTimestamp = 0;
    bool ordered = true;
    const auto messageProcessor = std::make_shared<TLightweightSessiondumpMessageProcessor>(
        factory->createMessageReader(io),
        [&](const TSessiondumpPacketInfo& packet, const TSessiondumpMessageInfo&, const std::function<QByteArray()>&) {
            ordered = ordered && packet.timestamp >= lastTimestamp;
            lastTimestamp = packet.timestamp;
            ++messages;
        });
    TSessiondumpProcessor processor(factory->createReader(io),
                                    factory->createPacketProcessor(factory->createPacketReader(io), messageProcessor));
    processor.start();
    EXPECT_EQ(processor.metadata().version, 7);
    EXPECT_EQ(processor.metadata().characterName, QStringLiteral("Player"));
    while (processor.hasNextPacket())
        processor.processNextPacket();
    EXPECT_GT(messages, 100);
    EXPECT_TRUE(ordered);
    EXPECT_TRUE(processor.atEnd());
}
