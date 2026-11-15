#pragma once

#include "network/gameserverconnection.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QPointer>
#include <QString>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace tibia {
namespace network {


/// What the login message needs from the session (sent once the server's
/// LoginChallenge arrives).
struct TGameserverLoginData
{
    std::string sessionKey;
    std::string characterName;
    std::string host;
    uint16_t port = 0;
    std::string recordTo;
};

/// CIP network::TGameserverNetworkPacketProcessor: sits between the game
/// connection and the protocol layer. Decoded packets go to
/// protocol::TProtocolServerMessageProcessor; queued client messages come back
/// here to be framed and written. One live instance per game session.
class TGameserverNetworkPacketProcessor final : public cip::tibia::network::TGameserverNetworkPacketProcessor
{
    Q_OBJECT
public:
    explicit TGameserverNetworkPacketProcessor(QObject* parent = nullptr);
    ~TGameserverNetworkPacketProcessor() override;

    /// The processor of the running session, or null.
    static TGameserverNetworkPacketProcessor* live();

    /// Opens the connection to the game server.
    void connectToGameserver(const TGameserverLoginData& login);
    /// Replays a recorded session: packets come from deliverPacket, sends are dropped.
    void beginReplay();
    /// Closes the connection; no error is reported.
    void disconnect();
    /// Ends this session's processor: disconnects, stops being live() and is
    /// deleted once control returns to the event loop.
    void retire();
    /// Ends the session as a connection failure (`clean` = peer close, no error dialog).
    void fail(const QString& message, bool clean);

    bool isConnected() const;
    bool isConnecting() const;
    bool isReplay() const { return m_replay; }
    /// Milliseconds since the last packet arrived, -1 when not connected.
    qint64 millisSinceRead() const;

    /// Answers LoginChallenge with the login message (opens XTEA afterwards).
    void sendLogin(uint32_t challengeTimestamp, uint8_t challengeRandom);
    /// The session XTEA key (the secondary connection shares it).
    std::array<uint32_t, 4> xteaKey() const { return m_xteaKey; }

    /// Encoded client message from the protocol queue → wire.
    void writeClientFrame(const QByteArray& frame);
    /// Product frames (extended opcode transport, character switch) → wire.
    void writeProductFrame(const QByteArray& frame);

    /// A decoded server packet (connection or replay).
    void deliverPacket(const uint8_t* data, size_t size);

    /// The product extended-opcode transport was opened by the server.
    void setProductTransportEnabled(bool enabled) { m_productTransport = enabled; }
    bool productTransportEnabled() const { return m_productTransport; }

    /// WorldEntered starts the game once per login (and once per character switch).
    bool gameInitialized() const { return m_gameInitialized; }
    void setGameInitialized(bool initialized) { m_gameInitialized = initialized; }

    TGameserverNetworkPacketConnection* connection() const { return m_connection; }

    int receivedPackets() const { return m_receivedPackets; }
    int receivedBytes() const { return m_receivedBytes; }

    void onNewPacketDataAvailable() override { }

private:
    friend class TGameserverNetworkPacketConnection;
    void connectionEstablished();
    void connectionFailed(const QString& message, bool clean);
    void write(const QByteArray& frame, bool raw);
    void releaseConnection();

    TGameserverLoginData m_login;
    QPointer<TGameserverNetworkPacketConnection> m_connection;
    std::array<uint32_t, 4> m_xteaKey{};
    bool m_disconnected = false;
    bool m_replay = false;
    bool m_firstPacket = true;
    bool m_productTransport = false;
    bool m_gameInitialized = false;
    int m_receivedPackets = 0;
    int m_receivedBytes = 0;
};

}
}
