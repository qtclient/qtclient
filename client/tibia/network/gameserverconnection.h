#pragma once

#include "network/gameservernetworkpacket.h"

#include <tibia/game/tgamenotify.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

namespace tibia {
namespace authentication {
class TAuthenticationAndEncryptionInfo;
}
namespace shared {
class TCompressionHelper;
class TRsaHelper;
}
namespace network {

class TGameserverDualConnection;
class TGameserverNetworkPacketRawDataProcessor;
class TGameserverNetworkPacketSequenceFlowProcessor;
class TUnencryptedRawMessageStream;

/// What the login needs from the session.
struct TGameserverLoginData
{
    std::string sessionKey;
    std::string characterName;
    std::string host;
    uint16_t port = 0;
    std::string recordTo;
};

/// Installs the login RSA key (Crystal's modulus) into loginRsaHelper().
void installGameServerConnection();
/// Encrypts the RSA blocks of the login messages.
shared::TRsaHelper& loginRsaHelper();

/// The network part of the running game session, built the way CipSoft's
/// TGameserverGameSession builds it: two game server connections under a
/// TGameserverDualConnection, the TAuthenticationAndEncryptionInfo, the
/// sequence flow processor and the raw data processor. Incoming packets are
/// put in order, decoded and handed to the protocol layer; encoded client
/// messages become packets for the dual connection. The session's network
/// tick (CipSoft: the session reads every connection and checks the dual
/// connection's state each tick) is a timer here. One live instance per session.
class TGameserverSessionNetwork final : public QObject
{
    Q_OBJECT
public:
    explicit TGameserverSessionNetwork(QObject* parent = nullptr);
    ~TGameserverSessionNetwork() override;

    /// The network of the running session, or null.
    static TGameserverSessionNetwork* live();

    /// Connects to the game server.
    void connectToGameserver(const TGameserverLoginData& login);
    /// Replays a recorded session: packets come from deliverPacket, sends are dropped.
    void beginReplay();
    /// Closes the connections; no error is reported.
    void disconnect();
    /// Ends this session's network: disconnects, stops being live() and is
    /// deleted once control returns to the event loop.
    void retire();
    /// Ends the session as a connection failure (`clean` = peer close, no error dialog).
    void fail(const QString& message, bool clean);

    bool isConnected() const;
    bool isConnecting() const;
    bool isReplay() const { return m_replay; }
    /// Milliseconds since the last packet arrived, -1 when not connected.
    qint64 millisSinceRead() const;

    /// Answers a login challenge (CIP TLoginProtocolMessageHandler): the login,
    /// or the secondary login once the server allowed a secondary connection.
    void sendLogin(uint32_t challengeTimestamp, uint8_t challengeRandom);

    /// Encoded client message from the protocol queue -> packet -> wire.
    void writeClientFrame(const QByteArray& frame);
    /// Product frames (extended opcode transport) -> wire.
    void writeProductFrame(const QByteArray& frame);

    /// A decoded server packet (replay).
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
    void buildNetworkStack();
    void releaseNetworkStack();
    void onPacketReady(TGameserverNetworkPacket packet);
    void onReadyForSecondaryConnection(const QString& token);
    void processServerData(const QByteArray& data);
    void sendFrame(const QByteArray& frame);
    void recordInput(const QByteArray& data);
    void recordOutput(const QByteArray& data);

    TGameserverLoginData m_login;
    std::shared_ptr<authentication::TAuthenticationAndEncryptionInfo> m_authenticationAndEncryptionInfo;
    std::shared_ptr<TGameserverDualConnection> m_dualConnection;
    std::shared_ptr<TUnencryptedRawMessageStream> m_serverMessageStream;
    std::shared_ptr<shared::TCompressionHelper> m_compressionHelper;
    std::shared_ptr<TGameserverNetworkPacketSequenceFlowProcessor> m_sequenceFlowProcessor;
    std::shared_ptr<TGameserverNetworkPacketRawDataProcessor> m_rawDataProcessor;
    QTimer m_networkTick;
    QElapsedTimer m_readTimer;
    tibia::game::SoftHook m_readyForSecondaryConnectionHook;
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
