#pragma once

#include <cip_gen.h>

#include <QByteArray>
#include <QString>
#include <QTimer>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#if defined(Q_OS_WASM)
#include <emscripten/websocket.h>
#else
#include <QTcpSocket>
#endif

namespace tibia::network {

/// Second connection to the same game host for CIP connection-stability optimisation
/// (CIP: TGameserverDualConnection). Opens the socket and sends SecondaryLogin
/// (C 0x0B) + ConnectionPingBack (C 0x1C) when the option is on and `g_game`
/// emits `onReadyForSecondaryConnection`. Toggle via
/// `connectionStabilityOptimizationEnabled`.
///
/// Desktop uses TCP (`QTcpSocket`). WASM uses a raw browser WebSocket to Crystal's
/// `websocketGamePort` (same binary framing as primary `WebConnection`) — Qt's
/// `QTcpSocket` wasm tunnel is not compatible with that endpoint.
///
/// After SecondaryLogin, XTEA keys are copied from the primary ProtocolGame so
/// this socket can decrypt frames (shared session; no second player). Crystal
/// OT classic uses the secondary as a **stability** socket (SecondaryLogin +
/// ConnectionPingBack); optional fan-out copies are decrypted then discarded so
/// state is not double-applied (CIP protobuf load-split is out of scope).
class TGameserverDualConnection final : public cip::tibia::network::TGameserverDualConnection
{
    Q_OBJECT
public:
    explicit TGameserverDualConnection(QObject* parent = nullptr);
    ~TGameserverDualConnection() override;

    /// Game world endpoint used for the secondary socket (same as primary).
    void setEndpoint(const QString& host, int port);
    /// Drop token + socket (logout / leave world).
    void reset();

    void checkForStateChange() override;
    /// CIP slot (not yet emitted by cipgen); wired from ClientOptionsNotifier.
    void onOptimizeConnectionStabilityOptionChanged();

private:
    void onReadyForSecondaryConnection(const std::string& token);
    void openSecondaryIfNeeded();
    void closeSecondary();
    void writeRaw(const QByteArray& payload);
    void sendSecondaryLogin();
    void sendConnectionPingBack();
    void onTransportBytes(const QByteArray& chunk);
    void adoptPrimaryXteaKeys();
    void processRecvBuffer();
    void deliverSecondaryDecrypted(quint32 checksum, QByteArray body);
    bool decryptXteaInPlace(QByteArray& body) const;
    bool optimizationEnabled() const;
    bool transportConnected() const;

#if defined(Q_OS_WASM)
    void openWebSocket();
    QString webSocketUrl() const;
    EMSCRIPTEN_WEBSOCKET_T m_websocket = 0;
    bool m_wsConnected = false;
#else
    std::unique_ptr<QTcpSocket> m_socket;
#endif

    QString m_host;
    int m_port = 0;
    QString m_token;
    bool m_enabled = true;
    bool m_challengeSeen = false;
    bool m_xteaEnabled = false;
    std::array<uint32_t, 4> m_xteaKey{};
    QByteArray m_recvBuffer;
    QTimer m_pingTimer;
    int m_readyHook = 0;
};

}
