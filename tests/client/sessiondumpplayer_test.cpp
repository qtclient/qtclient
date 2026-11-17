// sessiondump::TClientSessiondumpRecorder, TSessiondumpAsyncLoader and TSessiondumpPlayer:
// a recorded partial dump reads back through the loader and plays through the player.

#include "sessiondump/clientsessiondumprecorder.h"
#include "sessiondump/sessiondumpasyncloader.h"
#include "sessiondump/sessiondumpfactory.h"
#include "sessiondump/sessiondumpplayer.h"
#include "sessiondump/sessiondumpprogressclockservice.h"
#include "sessiondump/sessiondumpreader.h"
#include "shared/iodevicereader.h"

#include <protocol.pb.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

using namespace tibia;
using namespace tibia::sessiondump;
namespace pp = tibia::protobuf::protocol;

namespace {

std::shared_ptr<pp::GameserverMessage> message(int type, uint32_t packetNumber)
{
    auto m = std::make_shared<pp::GameserverMessage>();
    m->set_type(type);
    m->mutable_meta_data()->set_packet_number(packetNumber);
    return m;
}

std::shared_ptr<pp::GameserverMessage> loginSuccess(uint32_t playerId, uint32_t packetNumber)
{
    auto m = message(23, packetNumber);
    m->MutableExtension(pp::GameserverMessageLoginSuccess::gameserver_message_login_success)->set_player_id(playerId);
    return m;
}

QByteArray recordedDump()
{
    TClientSessiondumpRecorder recorder(60);
    recorder.start(QStringLiteral("Knight"), QStringLiteral("Antica"), QStringLiteral("key"));
    recorder.handleAnyMessage(loginSuccess(4711, 1));
    recorder.handleAnyMessage(message(15, 1));
    recorder.handleAnyMessage(message(10, 2));
    return recorder.sessiondump();
}

void spin(const std::function<bool()>& done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 10000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

}

TEST(ClientSessiondumpRecorder, PartialDumpHasMetadataAndOnePacketPerNetworkPacket)
{
    const QByteArray dump = recordedDump();
    auto buffer = std::make_shared<QBuffer>();
    buffer->setData(dump);
    const auto device = std::make_shared<shared::TIODeviceReader>(buffer, QDataStream::LittleEndian);
    const auto factory = std::make_shared<TSessiondumpFactory>();
    const auto reader = factory->createReader(device);
    const TSessiondumpMetadata metadata = reader->metadata();
    ASSERT_TRUE(metadata.read);
    EXPECT_EQ(metadata.version, 7);
    EXPECT_EQ(metadata.recordingMode, 1u);
    EXPECT_EQ(metadata.characterName, QStringLiteral("Knight"));
    EXPECT_EQ(metadata.worldName, QStringLiteral("Antica"));
    EXPECT_EQ(metadata.sessionKey, QStringLiteral("key"));
    // The partial dump takes the segment metadata at start and at each rollover;
    // the player id of LoginSuccess reaches it with the next segment.
    EXPECT_EQ(metadata.playerId, 0u);

    int packets = 0;
    const auto packetReader = reader->packetReader();
    while (!packetReader->atEnd() && device->bytesAvailable() > 0) {
        packetReader->readHeader();
        if (!packetReader->hasCompletePayload())
            break;
        ++packets;
        packetReader->skipPacket();
    }
    EXPECT_EQ(packets, 2);
}

TEST(SessiondumpPlayer, PlaysARecordedDump)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("recorded.dmp"));
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(recordedDump());
    }
    auto loader = std::make_shared<TSessiondumpAsyncLoader>(QUrl::fromLocalFile(path));
    auto clock = std::make_shared<TSessiondumpProgressClockService>();
    TSessiondumpPlayer player(loader, clock);
    bool metadataLoaded = false;
    bool loaded = false;
    std::vector<int> types;
    int preprocessed = 0;
    QObject::connect(loader.get(), &TSessiondumpAsyncLoader::sessiondumpMetadataLoaded, [&] { metadataLoaded = true; });
    QObject::connect(&player, &TSessiondumpPlayer::finishedLoadingSessiondump, [&] { loaded = true; });
    QObject::connect(&player, &TSessiondumpPlayer::preprocessingMessageAvailable, [&] { ++preprocessed; });
    QObject::connect(&player, &TSessiondumpPlayer::gameserverMessageAvailable,
                     [&](const TGameserverMessagePointer& m) { types.push_back(m->type()); });
    player.start();
    spin([&] { return loaded; });
    ASSERT_TRUE(metadataLoaded);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(preprocessed, 3);
    EXPECT_EQ(player.metadata().characterName, QStringLiteral("Knight"));

    player.skipTo(1.0, 1'000'000'000);
    player.processSessiondump();
    EXPECT_EQ(types, (std::vector<int>{ 23, 15, 10 }));
    EXPECT_FALSE(player.isSkipping());
}

TEST(SessiondumpPlayer, Playspeeds)
{
    auto loader = std::make_shared<TSessiondumpAsyncLoader>(QUrl());
    auto clock = std::make_shared<TSessiondumpProgressClockService>();
    TSessiondumpPlayer player(loader, clock);
    player.setPlayspeed(3.0);
    player.faster();
    EXPECT_DOUBLE_EQ(player.currentPlayspeed(), 6.0);
    player.fastest();
    EXPECT_DOUBLE_EQ(player.currentPlayspeed(), 8.0);
    player.setDuration(100);
    player.setLoadedDuration(50);
    EXPECT_DOUBLE_EQ(player.loadingProgress(), 0.5);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
