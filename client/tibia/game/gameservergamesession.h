#pragma once

#include "game/gamesessionbase.h"
#include "game/gameservergamesessionmetainfo.h"

#include "client/gamecore.h"
#include "network/dualconnectionurlinfo.h"

#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QString>
#include <QTimer>

#include <memory>

#include <string_view>
#include <vector>

namespace tibia::sessiondump {
class TClientSessiondumpRecorder;
}
namespace tibia::input {
class TPlayerMovementIntentHandler;
}
namespace tibia::authentication {
class TAuthenticationAndEncryptionInfo;
class TLoginProtocolMessageHandler;
}
namespace tibia::shared {
class TCompressionHelper;
}
namespace tibia::network {
class TGameserverDualConnection;
class TGameserverNetworkPacket;
class TGameserverNetworkPacketRawDataProcessor;
class TGameserverNetworkPacketSequenceFlowProcessor;
class TGameserverSessionNetwork;
class TUnencryptedRawMessageStream;
}

namespace tibia {
namespace game {

/// Hub login teardown (`forceLogout` / `cancelLogin`). Only
/// `TGameserverGameSession`, `TSessiondumpGameSession`, and `gamecore::terminate`
/// may call this — the only path that touches `g_game` for disconnect.
void hubDisconnectNow();

/// Live gameserver session (CIP: TGameserverGameSession), one per login. It owns
/// its network, as CipSoft's does: the dual game server connection, the packet
/// processors, the authentication and encryption info and the login protocol
/// message handler, built when the session is initialized; connectToGameserver
/// logs in with the meta info's login data. The session's handlers send their
/// client messages through its protocol queue (connectProtocolMessageHandlers).
class TGameserverGameSession final : public TGameSessionBase
{
    Q_OBJECT
public:
    explicit TGameserverGameSession(QObject* parent = nullptr);
    ~TGameserverGameSession() override;

    /// Builds the session from a TGameserverGameSessionMetaInfo: storages,
    /// handlers, providers, the network stack, the login data, the session dump
    /// recorder, then the protocol and game action connections.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    void startSession() override;
    /// Connects with the meta info's login data and logs in; progress arrives
    /// as signals.
    void connectToGameserver();
    /// Asks the server to log the character out (it may refuse, e.g. in fight).
    void requestLogout();
    /// Drops the connection without waiting for the server.
    void disconnectNow() override;
    /// The game client's clock.
    std::shared_ptr<shared::IClockService> clockService() const override;
    /// One network tick: the connections read what arrived and the dual
    /// connection checks for a state change.
    void processNetwork();
    /// Which of the session's connections are in use.
    network::EConnectionsUsed connectionsUsed() const;

    bool isOnline() const;
    QString characterName() const;
    QString worldName() const;

    /// The session's login protocol message handler.
    const std::shared_ptr<authentication::TLoginProtocolMessageHandler>& loginProtocolMessageHandler() const
    {
        return m_loginProtocolMessageHandler;
    }

    /// The player's movement intent handler sends through the session's player
    /// protocol message handler, which hands it its snap backs and stops.
    /// qtclient: the intent handler belongs to the session state, which calls
    /// this for each new one.
    void connectPlayerMovementIntentHandler(input::TPlayerMovementIntentHandler* Handler);

    /// Session lifecycle side effects (notify bus + session state). `tibia::game::process*`
    /// forwards here when a live session is registered; static fallbacks keep
    /// sessiondump / headless paths working without `g_game` bodies in game.cpp.
    static void wireProcessLoginError(std::string_view error);
    static void wireProcessLoginAdvice(std::string_view message);
    static void wireProcessLoginWait(std::string_view message, uint8_t time);
    static void wireProcessWorldEntered();
    /// Host: a death stops the own walk and freezes the scene.
    static void wireProcessDeath();
    static void wireProcessPing();
    static void wireProcessPingBack();

Q_SIGNALS:
    /// The server wants a newer client (UpdateNeeded).
    void clientOutOfDate();

protected:
    /// The base's handlers, then the login protocol message handler, whose
    /// login outcome the session reports as its own signals.
    void createProtocolMessageHandlers() override;
    /// The session dump recorder and the base's links, then the login handler's
    /// messages and every handler's client messages to the protocol queue.
    void connectProtocolMessageHandlers() override;

protected Q_SLOTS:
    /// The login allows the secondary connection.
    void onGameReadyForSecondaryConnection();

private:
    /// The two game server connections under a dual connection, the server
    /// message stream, the raw data and sequence flow processors, and their
    /// connections to each other, to the protocol queue and to the session.
    void createNetworkStack();
    /// A packet in order from the sequence flow processor: decoded and handed
    /// to the protocol layer.
    void onPacketReady(network::TGameserverNetworkPacket packet);
    /// An encoded client message from the protocol queue: a packet for the
    /// dual connection while it uses a connection.
    void sendClientFrame(const QByteArray& frame);
    /// Closes the connections and stops the network tick.
    void closeConnection();
    /// The connection ended (`clean`: the server closed it).
    void onConnectionLost(const QString& message, bool clean);

    std::shared_ptr<authentication::TLoginProtocolMessageHandler> m_loginProtocolMessageHandler;
    /// Host: qtclient's server packets go through the classic parser, which
    /// reads them from this stream (CipSoft's server packet processor).
    std::shared_ptr<network::TUnencryptedRawMessageStream> m_serverMessageStream;
    std::shared_ptr<network::TGameserverNetworkPacketRawDataProcessor> m_rawDataProcessor;
    std::shared_ptr<authentication::TAuthenticationAndEncryptionInfo> m_authenticationAndEncryptionInfo;
    GameserverCredentials m_loginData;
    std::shared_ptr<network::TGameserverDualConnection> m_dualConnection;
    std::shared_ptr<network::TGameserverNetworkPacketSequenceFlowProcessor> m_sequenceFlowProcessor;
    /// CIP's recorder of the session's last server messages, for crash reports.
    std::shared_ptr<sessiondump::TClientSessiondumpRecorder> m_sessiondumpRecorder;
    shared::TQtConnectionManager m_connectionManager;
    std::shared_ptr<shared::TCompressionHelper> m_compressionHelper;
    /// Host: the network tick (CipSoft's game client runs processNetwork every tick).
    QTimer m_networkTick;
    /// Host: the classic session state's view of this session's connection.
    std::weak_ptr<network::TGameserverSessionNetwork> m_sessionNetwork;
    /// Host: the client message links, made again when the game window relinks
    /// the protocol queue.
    shared::TQtConnectionManager m_clientMessageConnections;
    std::vector<tibia::game::SoftHook> m_hooks;
};

}
}
