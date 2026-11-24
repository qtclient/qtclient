#include "network/networkqualityprotocolmessagehandler.h"

#include "iclockservice.h"

#include <protocol.pb.h>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace tibia::network {

namespace {

/// Ping again at the latest this long after the last ping back.
constexpr qint64 PingInterval = 15000;
/// A ping is overdue this long after it was due.
constexpr qint64 PingTimeout = 15000;
/// Each outstanding ping delays the next one by this much.
constexpr qint64 OutstandingPingDelay = 5000;
/// Any other message moves the next ping to at most this far ahead.
constexpr qint64 ActivityPingDelay = 5000;
constexpr uint32_t MaxOutstandingPings = 3;
/// An outstanding ping counts as the round trip once it waited this long.
constexpr qint64 LatencyEstimateThreshold = 1000;
constexpr qint64 LatencyEstimateStep = 500;

/// The connection a server message came on (the meta data's connection value).
EConnectionType connectionTypeFromProtocol(const int Value)
{
    static const std::map<int, EConnectionType> Types{
        { 0, EConnectionType::Primary },
        { 1, EConnectionType::Secondary },
    };
    const auto type = Types.find(Value);
    if (type == Types.end())
        throw std::runtime_error("unhandled enum value");
    return type->second;
}

/// The connection value a connection ping back carries.
int connectionTypeToProtocol(const EConnectionType Type)
{
    static const std::map<EConnectionType, int> Values{
        { EConnectionType::Primary, 0 },
        { EConnectionType::Secondary, 1 },
    };
    const auto value = Values.find(Type);
    if (value == Values.end())
        throw std::runtime_error("unhandled enum value");
    return value->second;
}

}

TNetworkQualityProtocolMessageHandler::TNetworkQualityProtocolMessageHandler(
    const std::shared_ptr<shared::IClockService>& ClockService)
    : QObject(nullptr)
    , m_clockService(ClockService)
{
}

TNetworkQualityProtocolMessageHandler::~TNetworkQualityProtocolMessageHandler() = default;

void TNetworkQualityProtocolMessageHandler::handleAnyMessage(
    std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage)
{
    using ::tibia::protobuf::protocol::GameserverMessagePing;
    using ::tibia::protobuf::protocol::GameserverMessagePingBack;
    const qint64 now = m_clockService->nowMillis();
    const bool ping = pGameserverMessage->HasExtension(GameserverMessagePing::gameserver_message_ping);
    if (ping) {
        const auto& metaData = pGameserverMessage->meta_data();
        Q_EMIT pingMessageReceived(metaData.packet_number(), connectionTypeFromProtocol(metaData.value3()));
    }
    if (!pGameserverMessage->HasExtension(GameserverMessagePingBack::gameserver_message_ping_back) && !ping
        && now < m_nextPingTime) {
        m_nextPingTime = std::min(now + ActivityPingDelay, m_nextPingTime);
        m_outstandingPings = 0;
        m_timeoutTime = m_nextPingTime + PingTimeout;
    }
    if (!m_pingTimeout)
        return;
    m_pingTimeout = false;
    Q_EMIT pingTimeoutRecovered();
}

void TNetworkQualityProtocolMessageHandler::reset()
{
    m_latency = TNetworkLatency{};
    m_outstandingPings = 0;
    const qint64 now = m_clockService->nowMillis();
    m_nextPingTime = now;
    m_timeoutTime = now + PingTimeout;
    m_pingTimeout = false;
}

void TNetworkQualityProtocolMessageHandler::handlePingMessage()
{
    sendPingBack();
}

void TNetworkQualityProtocolMessageHandler::handlePingBackMessage()
{
    const qint64 now = m_clockService->nowMillis();
    m_nextPingTime = now + PingInterval;
    m_timeoutTime = now + PingInterval + PingTimeout;
    if (m_outstandingPings > 1)
        --m_outstandingPings;
    if (m_pingSendTimes.empty())
        return;
    const qint64 sent = m_pingSendTimes.front();
    m_pingSendTimes.pop_front();
    m_latency = TNetworkLatency{ now - sent, true };
}

void TNetworkQualityProtocolMessageHandler::sendPing()
{
    Q_EMIT sendPingMessage(::tibia::protobuf::protocol::GameclientMessagePing());
}

void TNetworkQualityProtocolMessageHandler::sendPingBack()
{
    Q_EMIT sendPingBackMessage(::tibia::protobuf::protocol::GameclientMessagePingBack());
}

void TNetworkQualityProtocolMessageHandler::sendConnectionPingBack(const EConnectionType ConnectionType)
{
    ::tibia::protobuf::protocol::GameclientMessageConnectionPingBack message;
    message.set_connection_type(connectionTypeToProtocol(ConnectionType));
    Q_EMIT sendConnectionPingBackMessage(message);
}

void TNetworkQualityProtocolMessageHandler::update()
{
    const qint64 now = m_clockService->nowMillis();
    if (m_outstandingPings < MaxOutstandingPings
        && m_nextPingTime + qint64(m_outstandingPings) * OutstandingPingDelay <= now) {
        m_timeoutTime = m_nextPingTime + PingTimeout;
        m_pingSendTimes.push_back(now);
        ++m_outstandingPings;
        sendPing();
    }
    if (!m_pingSendTimes.empty()) {
        const qint64 waiting = now - m_pingSendTimes.front();
        if (waiting > LatencyEstimateThreshold)
            m_latency = TNetworkLatency{ (waiting / LatencyEstimateStep) * LatencyEstimateStep, false };
    }
    if (now < m_timeoutTime || m_pingTimeout)
        return;
    m_pingTimeout = true;
    Q_EMIT pingTimeoutOccurred();
}

}
