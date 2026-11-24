#pragma once

#include "network/dualconnectionurlinfo.h"

#include <QObject>
#include <QtGlobal>

#include <cstdint>
#include <deque>
#include <memory>

namespace tibia::shared {
class IClockService;
}
namespace tibia::protobuf::protocol {
class GameserverMessage;
class GameclientMessagePing;
class GameclientMessagePingBack;
class GameclientMessageConnectionPingBack;
}

namespace tibia {
namespace network {

/// The sequence number of a game server packet.
using TPacketSequenceNumber = uint32_t;

/// The session's round trip time: measured from the last ping's answer, or,
/// while a ping is unanswered for more than a second, the time it has been
/// waiting (rounded down to half seconds).
struct TNetworkLatency
{
    qint64 milliseconds = 0;
    /// True when the time is a measured round trip.
    bool measured = false;
};

/// CIP network::TNetworkQualityProtocolMessageHandler: keeps the game server
/// connection measured. The session's tick sends a ping when one is due (at
/// most three unanswered), the server's ping back gives the round trip time,
/// and a ping unanswered for too long raises a ping timeout until any message
/// arrives again. The server's pings are answered with a ping back.
class TNetworkQualityProtocolMessageHandler : public QObject
{
    Q_OBJECT
public:
    explicit TNetworkQualityProtocolMessageHandler(const std::shared_ptr<shared::IClockService>& ClockService);
    ~TNetworkQualityProtocolMessageHandler() override;

    /// Any server message: a ping is announced with its packet and connection;
    /// any other message but a ping back moves a later next ping to at most
    /// five seconds from now; a ping timeout is over.
    virtual void handleAnyMessage(std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage);
    /// The login succeeded: no round trip yet, nothing outstanding, the next
    /// ping is due at once.
    virtual void reset();
    /// The server pinged: answer it.
    virtual void handlePingMessage();
    /// The server answered the oldest outstanding ping.
    virtual void handlePingBackMessage();
    virtual void sendPing();
    virtual void sendPingBack();
    /// Answers a connection ping on the connection it came on.
    virtual void sendConnectionPingBack(tibia::network::EConnectionType ConnectionType);

    /// The session's network tick while logged in: sends a due ping, estimates
    /// the round trip of an outstanding one and raises the ping timeout.
    void update();
    TNetworkLatency latency() const { return m_latency; }

Q_SIGNALS:
    void sendPingBackMessage(const ::tibia::protobuf::protocol::GameclientMessagePingBack& PingBackMessage);
    void sendPingMessage(const ::tibia::protobuf::protocol::GameclientMessagePing& PingMessage);
    void sendConnectionPingBackMessage(const ::tibia::protobuf::protocol::GameclientMessageConnectionPingBack& PingBackMessage);
    void pingTimeoutOccurred();
    void pingTimeoutRecovered();
    void pingMessageReceived(tibia::network::TPacketSequenceNumber SequenceNumber, tibia::network::EConnectionType ConnectionType);

private:
    std::shared_ptr<shared::IClockService> m_clockService;
    TNetworkLatency m_latency;
    uint32_t m_outstandingPings = 0;
    qint64 m_nextPingTime = INT64_MAX;
    qint64 m_timeoutTime = INT64_MAX;
    bool m_pingTimeout = false;
    /// When each outstanding ping was sent, oldest first.
    std::deque<qint64> m_pingSendTimes;
};

}
}
