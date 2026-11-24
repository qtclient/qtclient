#include "network/networkqualityprotocolmessagehandler.h"
#include "network/unencryptedrawmessagestream.h"
#include "protocol/protobufclientmessagetranslator.h"
#include "protocol/protocolclientmessageprocessor.h"
#include "protocol/protocolwriter.h"
#include "shared/iodevicewriter.h"

#include "iclockservice.h"

#include <protocol.pb.h>

#include <gtest/gtest.h>

#include <QDateTime>

#include <memory>
#include <vector>

using tibia::network::EConnectionType;
using tibia::network::TNetworkQualityProtocolMessageHandler;
using namespace tibia::protobuf::protocol;

// Test double of the classic encoding (protocolciptypes.cpp is not linked): the type byte, then
// the connection of a connection ping back.
QByteArray tibia::protocol::TProtobufClientMessageTranslator::translateToClassic(
    const GameclientMessage& message, tibia::shared::TRsaHelper*, const tibia::protocol::TProtobufObjectPositionHelper*)
{
    QByteArray data(1, static_cast<char>(message.type()));
    if (message.HasExtension(GameclientMessageConnectionPingBack::gameclient_message_connection_ping_back))
        data.append(static_cast<char>(
            message.GetExtension(GameclientMessageConnectionPingBack::gameclient_message_connection_ping_back)
                .connection_type()));
    return data;
}

namespace {

class TFakeClock : public tibia::shared::IClockService
{
public:
    qint64 nowMillis() override { return now; }
    qint64 nowNanos() override { return now * 1000000; }
    qint64 nowMicros() override { return now * 1000; }
    QDateTime currentDateTime() override { return QDateTime::fromMSecsSinceEpoch(now); }

    qint64 now = 100000;
};

struct TRecorder
{
    explicit TRecorder(TNetworkQualityProtocolMessageHandler& handler)
    {
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::sendPingMessage,
                         [this](const GameclientMessagePing&) { ++pings; });
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::sendPingBackMessage,
                         [this](const GameclientMessagePingBack&) { ++pingBacks; });
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::sendConnectionPingBackMessage,
                         [this](const GameclientMessageConnectionPingBack& message) {
                             connectionPingBacks.push_back(message.connection_type());
                         });
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::pingTimeoutOccurred,
                         [this] { ++timeouts; });
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::pingTimeoutRecovered,
                         [this] { ++recoveries; });
        QObject::connect(&handler, &TNetworkQualityProtocolMessageHandler::pingMessageReceived,
                         [this](tibia::network::TPacketSequenceNumber sequenceNumber, EConnectionType type) {
                             receivedPings.push_back({ sequenceNumber, type });
                         });
    }

    int pings = 0;
    int pingBacks = 0;
    int timeouts = 0;
    int recoveries = 0;
    std::vector<int> connectionPingBacks;
    std::vector<std::pair<uint32_t, EConnectionType>> receivedPings;
};

std::shared_ptr<GameserverMessage> serverMessage(const bool ping, const bool pingBack, const uint32_t packet = 0,
                                                 const int connection = 0)
{
    auto message = std::make_shared<GameserverMessage>();
    if (ping)
        message->MutableExtension(GameserverMessagePing::gameserver_message_ping);
    if (pingBack)
        message->MutableExtension(GameserverMessagePingBack::gameserver_message_ping_back);
    message->mutable_meta_data()->set_packet_number(packet);
    message->mutable_meta_data()->set_value3(connection);
    return message;
}

}

TEST(NetworkQualityProtocolMessageHandler, PingsAtOnceAfterTheLoginAndMeasuresTheRoundTrip)
{
    auto clock = std::make_shared<TFakeClock>();
    TNetworkQualityProtocolMessageHandler handler(clock);
    TRecorder recorder(handler);
    handler.reset();
    handler.update();
    EXPECT_EQ(recorder.pings, 1);
    // A second ping waits five seconds per outstanding ping.
    clock->now += 4999;
    handler.update();
    EXPECT_EQ(recorder.pings, 1);
    clock->now += 1;
    handler.update();
    EXPECT_EQ(recorder.pings, 2);
    // Waiting more than a second is an estimate, rounded down to half seconds.
    EXPECT_FALSE(handler.latency().measured);
    EXPECT_EQ(handler.latency().milliseconds, 5000);

    clock->now += 200;
    handler.handlePingBackMessage();
    // The oldest ping is answered.
    EXPECT_TRUE(handler.latency().measured);
    EXPECT_EQ(handler.latency().milliseconds, 5200);
    // The next ping is due 15 seconds after the ping back (one still outstanding: + 5 s).
    clock->now += 19999;
    handler.update();
    EXPECT_EQ(recorder.pings, 2);
    clock->now += 1;
    handler.update();
    EXPECT_EQ(recorder.pings, 3);
}

TEST(NetworkQualityProtocolMessageHandler, AtMostThreePingsAreOutstanding)
{
    auto clock = std::make_shared<TFakeClock>();
    TNetworkQualityProtocolMessageHandler handler(clock);
    TRecorder recorder(handler);
    handler.reset();
    for (int step = 0; step < 20; ++step) {
        handler.update();
        clock->now += 5000;
    }
    EXPECT_EQ(recorder.pings, 3);
}

TEST(NetworkQualityProtocolMessageHandler, ATimeoutLastsUntilAMessageArrives)
{
    auto clock = std::make_shared<TFakeClock>();
    TNetworkQualityProtocolMessageHandler handler(clock);
    TRecorder recorder(handler);
    handler.reset();
    handler.update();
    clock->now += 14999;
    handler.update();
    EXPECT_EQ(recorder.timeouts, 0);
    clock->now += 1;
    handler.update();
    handler.update();
    EXPECT_EQ(recorder.timeouts, 1);
    handler.handleAnyMessage(serverMessage(false, false));
    EXPECT_EQ(recorder.recoveries, 1);
    handler.handleAnyMessage(serverMessage(false, false));
    EXPECT_EQ(recorder.recoveries, 1);
}

TEST(NetworkQualityProtocolMessageHandler, OtherMessagesBringTheNextPingCloser)
{
    auto clock = std::make_shared<TFakeClock>();
    TNetworkQualityProtocolMessageHandler handler(clock);
    TRecorder recorder(handler);
    handler.reset();
    handler.update();
    handler.handlePingBackMessage(); // next ping in 15 s
    clock->now += 1000;
    // A ping back or a ping does not move it.
    handler.handleAnyMessage(serverMessage(false, true));
    handler.handleAnyMessage(serverMessage(true, false, 7, 1));
    clock->now += 5000;
    handler.update();
    EXPECT_EQ(recorder.pings, 1);
    // Any other message: at most five seconds from now, nothing outstanding.
    handler.handleAnyMessage(serverMessage(false, false));
    clock->now += 4999;
    handler.update();
    EXPECT_EQ(recorder.pings, 1);
    clock->now += 1;
    handler.update();
    EXPECT_EQ(recorder.pings, 2);
    ASSERT_EQ(recorder.receivedPings.size(), 1u);
    EXPECT_EQ(recorder.receivedPings[0].first, 7u);
    EXPECT_EQ(recorder.receivedPings[0].second, EConnectionType::Secondary);
}

TEST(NetworkQualityProtocolMessageHandler, AnswersPingsAndConnectionPings)
{
    auto clock = std::make_shared<TFakeClock>();
    TNetworkQualityProtocolMessageHandler handler(clock);
    TRecorder recorder(handler);
    handler.handlePingMessage();
    EXPECT_EQ(recorder.pingBacks, 1);
    handler.sendConnectionPingBack(EConnectionType::Secondary);
    handler.sendConnectionPingBack(EConnectionType::Primary);
    EXPECT_EQ(recorder.connectionPingBacks, (std::vector<int>{ 1, 0 }));
}

TEST(ProtocolClientMessageProcessor, SendsAConnectionPingBackOnTheConnectionOfItsPing)
{
    using tibia::network::EConnectionsUsed;
    using tibia::network::TGameserverNetworkPacket;
    auto messageStream = std::make_shared<tibia::network::TUnencryptedRawMessageStream>();
    auto translator = std::make_shared<tibia::protocol::TProtobufClientMessageTranslator>(
        std::make_shared<tibia::protocol::TProtocolWriter>(
            std::make_shared<tibia::shared::TIODeviceWriter>(messageStream, QDataStream::LittleEndian)),
        nullptr, nullptr, tibia::appearances::TSpecialAppearancesForProtocolParsing());
    tibia::protocol::TProtocolClientMessageProcessor processor(translator, messageStream);
    GameclientMessage message;
    message.set_type(28);
    message.MutableExtension(GameclientMessageConnectionPingBack::gameclient_message_connection_ping_back)
        ->set_connection_type(1);
    TGameserverNetworkPacket packet = processor.processClientMessage(message);
    EXPECT_EQ(packet.encryptionType(), TGameserverNetworkPacket::Xtea);
    EXPECT_EQ(packet.connectionsUsed(), EConnectionsUsed::Secondary);
    EXPECT_EQ(packet.data(), QByteArray("\x1c\x01", 2));

    // The stream holds only the message being processed: the bytes read back are dropped.
    message.MutableExtension(GameclientMessageConnectionPingBack::gameclient_message_connection_ping_back)
        ->set_connection_type(0);
    packet = processor.processClientMessage(message);
    EXPECT_EQ(packet.connectionsUsed(), EConnectionsUsed::Primary);
    EXPECT_EQ(packet.data(), QByteArray("\x1c\x00", 2));
    EXPECT_EQ(messageStream->size(), 0);

    GameclientMessage login;
    login.set_type(11);
    packet = processor.processClientMessage(login);
    EXPECT_EQ(packet.encryptionType(), TGameserverNetworkPacket::LoginRsa);
    EXPECT_EQ(packet.connectionsUsed(), EConnectionsUsed::None);

    GameclientMessage walk;
    walk.set_type(101);
    packet = processor.processClientMessage(walk);
    EXPECT_EQ(packet.data(), QByteArray("\x65", 1));
    EXPECT_EQ(packet.encryptionType(), TGameserverNetworkPacket::Xtea);
    EXPECT_EQ(packet.connectionsUsed(), EConnectionsUsed::PrimaryAndSecondary);
}
