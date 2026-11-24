#pragma once

#include "game/gamesessionbase.h"
#include "game/gameservergamesessionmetainfo.h"

#include "authentication/gameserverloginprocesscontroller.h"

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
class TPlayerMovementGameActionHandler;
class TPlayerMovementIntentHandler;
}
namespace tibia::authentication {
class TAuthenticationAndEncryptionInfo;
class TLoginProtocolMessageHandler;
}
namespace tibia::shared {
class TCompressionHelper;
}
namespace tibia::protocol {
class TProtocolClientMessageProcessor;
class TProtocolServerPacketProcessor;
}
namespace tibia::network {
class TNetworkQualityProtocolMessageHandler;
struct TNetworkLatency;
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
/// hands the meta info's login data to its game server login process
/// controller. The session's handlers send their
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
    /// The game server login process controller logs in with the meta info's
    /// login data; progress arrives as signals.
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
    network::EConnectionsUsed connectionsUsed() const override;
    /// The connection's round trip time (the network quality handler's).
    network::TNetworkLatency latency() const override;

    bool isOnline() const;
    QString characterName() const;
    QString worldName() const;

    /// The controller that connects the session to its world's game server.
    const std::shared_ptr<authentication::TGameserverLoginProcessController>& gameserverLoginProcessController() const
    {
        return m_gameserverLoginProcessController;
    }
    /// The session's login protocol message handler.
    const std::shared_ptr<authentication::TLoginProtocolMessageHandler>& loginProtocolMessageHandler() const
    {
        return m_loginProtocolMessageHandler;
    }

    /// The session's player movement intent handler (walks, held directions,
    /// rotations of the own character).
    const std::shared_ptr<input::TPlayerMovementIntentHandler>& playerMovementIntentHandler() const
    {
        return m_playerMovementIntentHandler;
    }
    /// Once per frame: the player movement intent handler repeats a held
    /// direction and sends the next step of its path when it is due.
    void updatePlayerMovementIntentHandler() override;

    /// Session lifecycle side effects (notify bus + session state). `tibia::game::process*`
    /// forwards here when a live session is registered; static fallbacks keep
    /// sessiondump / headless paths working without `g_game` bodies in game.cpp.
    static void wireProcessLoginError(std::string_view error);
    static void wireProcessLoginAdvice(std::string_view message);
    static void wireProcessLoginWait(std::string_view message, uint8_t time);
    static void wireProcessWorldEntered();
    /// Host: a death stops the own walk and freezes the scene.
    static void wireProcessDeath();

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
    /// The base's links, then the player movement game action handler and the
    /// player movement intent handler with the game action router.
    void connectGameActionHandlers() override;
    /// The player's movement intent handler sends through the session's player
    /// protocol message handler, which hands it its snap backs and stops.
    void connectPlayerMovementIntentHandler();

protected Q_SLOTS:
    /// The base's, then a character that got the default sidebar configuration
    /// shows its special conditions in the HUD.
    void onCharacterConfigurationLoaded() const override;
    /// The login allows the secondary connection.
    void onGameReadyForSecondaryConnection();

private:
    /// The two game server connections under a dual connection, the server
    /// message stream, the raw data and sequence flow processors, and their
    /// connections to each other, to the protocol queue and to the session.
    void createNetworkStack();
    /// The game server login process controller over the dual connection and
    /// the authentication info, and the player movement intent handler with the
    /// player's creature (now or once the creature storage adds it).
    void createSessionControllers();
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
    std::shared_ptr<network::TNetworkQualityProtocolMessageHandler> m_networkQualityProtocolMessageHandler;
    std::shared_ptr<protocol::TProtocolClientMessageProcessor> m_clientMessageProcessor;
    /// The decoded bytes of the server packets, read by the server packet processor.
    std::shared_ptr<network::TUnencryptedRawMessageStream> m_serverMessageStream;
    std::shared_ptr<protocol::TProtocolServerPacketProcessor> m_serverPacketProcessor;
    std::shared_ptr<network::TGameserverNetworkPacketRawDataProcessor> m_rawDataProcessor;
    std::shared_ptr<authentication::TAuthenticationAndEncryptionInfo> m_authenticationAndEncryptionInfo;
    std::shared_ptr<authentication::TGameserverLoginProcessController> m_gameserverLoginProcessController;
    std::shared_ptr<input::TPlayerMovementGameActionHandler> m_playerMovementGameActionHandler;
    std::shared_ptr<input::TPlayerMovementIntentHandler> m_playerMovementIntentHandler;
    authentication::TGameserverLoginRequestData m_loginData;
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
    /// Host: the file the session's packets are recorded to (--record).
    QString m_recordTo;
    std::vector<tibia::game::SoftHook> m_hooks;
};

}
}
