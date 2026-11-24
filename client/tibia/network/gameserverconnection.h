#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

namespace tibia {
namespace shared {
class TRsaHelper;
}
namespace network {

/// Installs the login RSA key (Crystal's modulus) into loginRsaHelper().
void installGameServerConnection();
/// Encrypts the RSA blocks of the login messages.
shared::TRsaHelper& loginRsaHelper();

/// Host: the classic session state's view of the running session's game server
/// connection. The live game server session (game::TGameserverGameSession) owns
/// the network stack, as CipSoft's does; it reports its connection state here,
/// hands the decoded server data to the classic packet parser through it and
/// writes the frames the classic senders and the product transport put here.
/// A replay of a classic packet log has no network: its packets come from
/// deliverPacket and its client frames are dropped. One live instance at a time.
class TGameserverSessionNetwork final : public QObject
{
    Q_OBJECT
public:
    explicit TGameserverSessionNetwork(QObject* parent = nullptr);
    ~TGameserverSessionNetwork() override;

    /// A new instance that is retired (not deleted) when its last owner lets it
    /// go: that can happen inside one of the connection's own callbacks.
    static std::shared_ptr<TGameserverSessionNetwork> create();
    /// The connection of the running session, or null.
    static TGameserverSessionNetwork* live();

    /// The owning session writes the client frames and closes its connection.
    void setFrameWriter(std::function<void(const QByteArray&)> writer);
    void setDisconnector(std::function<void()> disconnector);
    /// The session's connections are in use (true) or none is (false).
    void setConnected(bool connected);
    /// A packet arrived (the "connection ok" timer restarts).
    void notePacketRead();
    /// Records the session's packets to `records/<recordTo>` (qtclient's packet log).
    void startRecording(const std::string& recordTo);

    /// Replays a recorded session: packets come from deliverPacket, sends are dropped.
    void beginReplay();
    /// Closes the connection; no error is reported.
    void disconnect();
    /// Ends this connection: disconnects, stops being live() and is deleted once
    /// control returns to the event loop.
    void retire();
    /// Ends the session as a connection failure (`clean` = peer close, no error dialog).
    void fail(const QString& message, bool clean);

    bool isConnected() const;
    bool isConnecting() const;
    bool isReplay() const { return m_replay; }
    /// Milliseconds since the last packet arrived, -1 when not connected.
    qint64 millisSinceRead() const;

    /// Encoded client message from the protocol queue -> the session's connection.
    void writeClientFrame(const QByteArray& frame);
    /// Product frames (extended opcode transport) -> the session's connection.
    void writeProductFrame(const QByteArray& frame);

    /// Decoded server data of the live connection: recorded and counted. False
    /// when it is not to be read (empty, too long, or the connection ended).
    bool noteServerData(const QByteArray& data);
    /// A decoded server packet (live or replay) for the classic packet parser.
    void deliverPacket(const uint8_t* data, size_t size);

    /// The product extended-opcode transport was opened by the server.
    void setProductTransportEnabled(bool enabled) { m_productTransport = enabled; }
    bool productTransportEnabled() const { return m_productTransport; }

    /// WorldEntered starts the game once per login (and once per character switch).
    bool gameInitialized() const { return m_gameInitialized; }
    void setGameInitialized(bool initialized) { m_gameInitialized = initialized; }

    int receivedPackets() const { return m_receivedPackets; }
    int receivedBytes() const { return m_receivedBytes; }

private:
    void sendFrame(const QByteArray& frame);
    void recordInput(const QByteArray& data);
    void recordOutput(const QByteArray& data);

    std::function<void(const QByteArray&)> m_frameWriter;
    std::function<void()> m_disconnector;
    QElapsedTimer m_readTimer;
    bool m_connected = false;
    bool m_disconnected = false;
    bool m_replay = false;
    bool m_productTransport = false;
    bool m_gameInitialized = false;
    int m_receivedPackets = 0;
    int m_receivedBytes = 0;
    bool m_skipFirstOutput = true;
    qint64 m_recordStart = 0;
    std::unique_ptr<std::ofstream> m_record;
};

}
}
