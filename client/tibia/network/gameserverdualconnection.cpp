#include "gameserverdualconnection.h"

#include "gameserverconnection.h"
#include "app/clientoptions.h"
#include "client/gamecore.h"

#include <tibia/world/game.h>
#include <client/protocolgame.h>
#include <framework/core/gameevents.h>

#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#endif

Q_LOGGING_CATEGORY(lcDualConn, "tibia.dualconnection")

namespace tibia::network {

namespace {

constexpr quint8 ClientSecondaryLogin = 0x0B;
constexpr quint8 ClientConnectionPingBack = 0x1C;

using client::gamecore::argString;

/// Little-endian u16 length prefix + UTF-8 (matches OTC OutputMessage::addString).
void appendString(QByteArray& out, const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    const auto len = static_cast<quint16>(qMin(utf8.size(), 0xffff));
    out.append(char(len & 0xff));
    out.append(char((len >> 8) & 0xff));
    out.append(utf8.constData(), int(len));
}

/// CIP 1405+ game-port frame: [u16 headerSize][u32 checksum][u8 padAmt][payload][zeros].
/// headerSize = (checksum+body-4)/8 with body padded to a multiple of 8.
QByteArray frameModern(const QByteArray& payload, const quint32 checksum)
{
    QByteArray body = payload;
    const auto padAmt = static_cast<quint8>(8 - (body.size() % 8) - 1);
    body.append(QByteArray(int(padAmt), '\0'));
    body.prepend(char(padAmt));

    QByteArray packet;
    packet.append(char(checksum & 0xff));
    packet.append(char((checksum >> 8) & 0xff));
    packet.append(char((checksum >> 16) & 0xff));
    packet.append(char((checksum >> 24) & 0xff));
    packet.append(body);

    const auto headerSize = static_cast<quint16>((packet.size() - 4) / 8);
    QByteArray out;
    out.append(char(headerSize & 0xff));
    out.append(char((headerSize >> 8) & 0xff));
    out.append(packet);
    return out;
}

#if defined(Q_OS_WASM)
bool isLoopbackOrLan(const QString& host)
{
    if (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1") || host == QLatin1String("0.0.0.0"))
        return true;
    if (host.startsWith(QLatin1String("192.168.")) || host.startsWith(QLatin1String("10.")))
        return true;
    if (host.startsWith(QLatin1String("172."))) {
        const QStringList parts = host.split(QLatin1Char('.'));
        if (parts.size() >= 2) {
            bool ok = false;
            const int oct = parts[1].toInt(&ok);
            if (ok && oct >= 16 && oct <= 31)
                return true;
        }
    }
    return false;
}
#endif

}

TGameserverDualConnection::TGameserverDualConnection(QObject* parent)
    : cip::tibia::network::TGameserverDualConnection(parent)
{
    m_enabled = optimizationEnabled();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("connectionStabilityOptimizationEnabled"))
            onOptimizeConnectionStabilityOptionChanged();
    });

    m_readyHook = g_gameEvents.subscribe("g_game", "onReadyForSecondaryConnection",
                                         [this](LuaObject*, const GameEvents::Args& args) {
                                             onReadyForSecondaryConnection(argString(args, 0).toStdString());
                                             return std::any{};
                                         });

    m_pingTimer.setInterval(5000);
    connect(&m_pingTimer, &QTimer::timeout, this, [this] {
        if (transportConnected())
            sendConnectionPingBack();
    });
}

TGameserverDualConnection::~TGameserverDualConnection()
{
    if (m_readyHook)
        g_gameEvents.unsubscribe(m_readyHook);
    closeSecondary();
}

void TGameserverDualConnection::setEndpoint(const QString& host, const int port)
{
    m_host = host;
    m_port = port;
    checkForStateChange();
}

void TGameserverDualConnection::reset()
{
    m_token.clear();
    m_host.clear();
    m_port = 0;
    m_xteaEnabled = false;
    m_xteaKey = {};
    m_recvBuffer.clear();
    closeSecondary();
}

bool TGameserverDualConnection::optimizationEnabled() const
{
    return app::ClientOptions::load().options().value(QLatin1String("connectionStabilityOptimizationEnabled")).toBool(true);
}

void TGameserverDualConnection::onOptimizeConnectionStabilityOptionChanged()
{
    m_enabled = optimizationEnabled();
    checkForStateChange();
}

void TGameserverDualConnection::onReadyForSecondaryConnection(const std::string& token)
{
    m_token = QString::fromStdString(token);
    checkForStateChange();
}

void TGameserverDualConnection::checkForStateChange()
{
    if (m_enabled && !m_token.isEmpty() && !m_host.isEmpty() && m_port > 0)
        openSecondaryIfNeeded();
    else
        closeSecondary();
}

bool TGameserverDualConnection::transportConnected() const
{
#if defined(Q_OS_WASM)
    return m_websocket > 0 && m_wsConnected;
#else
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
#endif
}

void TGameserverDualConnection::openSecondaryIfNeeded()
{
#if defined(Q_OS_WASM)
    if (m_websocket > 0)
        return;
#else
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
        return;
#endif

    m_challengeSeen = false;
    m_xteaEnabled = false;
    m_recvBuffer.clear();

#if defined(Q_OS_WASM)
    openWebSocket();
#else
    m_socket = std::make_unique<QTcpSocket>();
    connect(m_socket.get(), &QTcpSocket::readyRead, this, [this] {
        if (!m_socket)
            return;
        onTransportBytes(m_socket->readAll());
    });
    connect(m_socket.get(), &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        Q_EMIT error(m_socket ? m_socket->errorString() : tr("secondary connection error"));
    });
    connect(m_socket.get(), &QTcpSocket::disconnected, this, [this] {
        m_pingTimer.stop();
        m_challengeSeen = false;
        m_xteaEnabled = false;
        m_recvBuffer.clear();
        Q_EMIT disconnected();
    });

    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));
#endif
}

#if defined(Q_OS_WASM)
QString TGameserverDualConnection::webSocketUrl() const
{
    // Match WebConnection: loopback/LAN → ws; else page https / :443 / CF tunnel → wss.
    QString prefix = QStringLiteral("ws://");
    if (!isLoopbackOrLan(m_host)) {
        const bool forceWss = m_port == 443
            || m_host.contains(QLatin1String("trycloudflare.com"))
            || m_host.contains(QLatin1String("cfargotunnel.com"));
        const int pageIsHttps = EM_ASM_INT({
            try {
                return (typeof location !== 'undefined' && location.protocol === 'https:') ? 1 : 0;
            } catch (e) {
                return 0;
            }
        });
        if (forceWss || pageIsHttps)
            prefix = QStringLiteral("wss://");
    }
    return prefix + m_host + QLatin1Char(':') + QString::number(m_port);
}

void TGameserverDualConnection::openWebSocket()
{
    const QByteArray urlUtf8 = webSocketUrl().toUtf8();
    // Crystal does not echo Sec-WebSocket-Protocol — omit subprotocol (same as WebConnection).
    EmscriptenWebSocketCreateAttributes attributes = {
        urlUtf8.constData(),
        nullptr,
        EM_FALSE,
    };
    m_websocket = emscripten_websocket_new(&attributes);
    m_wsConnected = false;
    if (m_websocket < 1) {
        m_websocket = 0;
        Q_EMIT error(tr("secondary WebSocket create failed"));
        return;
    }

    emscripten_websocket_set_onopen_callback(m_websocket, this, [](int, const EmscriptenWebSocketOpenEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverDualConnection*>(userData);
        QMetaObject::invokeMethod(self, [self] {
            self->m_wsConnected = true;
        }, Qt::QueuedConnection);
        return EM_TRUE;
    });

    emscripten_websocket_set_onmessage_callback(m_websocket, this, [](int, const EmscriptenWebSocketMessageEvent* event, void* userData) -> EM_BOOL {
        if (!event || event->numBytes <= 0 || event->isText)
            return EM_TRUE;
        auto* self = static_cast<TGameserverDualConnection*>(userData);
        const QByteArray chunk(reinterpret_cast<const char*>(event->data), int(event->numBytes));
        QMetaObject::invokeMethod(self, [self, chunk] { self->onTransportBytes(chunk); }, Qt::QueuedConnection);
        return EM_TRUE;
    });

    emscripten_websocket_set_onerror_callback(m_websocket, this, [](int, const EmscriptenWebSocketErrorEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverDualConnection*>(userData);
        QMetaObject::invokeMethod(self, [self] {
            Q_EMIT self->error(tr("secondary WebSocket error"));
        }, Qt::QueuedConnection);
        return EM_TRUE;
    });

    emscripten_websocket_set_onclose_callback(m_websocket, this, [](int, const EmscriptenWebSocketCloseEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverDualConnection*>(userData);
        // DynCall runs on the browser main thread under PROXY_TO_PTHREAD — never
        // call emscripten_websocket_* here; defer like WebConnection::reportError.
        QMetaObject::invokeMethod(self, [self] {
            self->m_pingTimer.stop();
            self->m_challengeSeen = false;
            self->m_xteaEnabled = false;
            self->m_recvBuffer.clear();
            self->m_wsConnected = false;
            const EMSCRIPTEN_WEBSOCKET_T sock = self->m_websocket;
            self->m_websocket = 0;
            if (sock > 0) {
                emscripten_websocket_close(sock, 1000, "secondary peer close");
                emscripten_websocket_delete(sock);
            }
            Q_EMIT self->disconnected();
        }, Qt::QueuedConnection);
        return EM_TRUE;
    });
}
#endif

void TGameserverDualConnection::closeSecondary()
{
    m_pingTimer.stop();
    m_challengeSeen = false;
    m_xteaEnabled = false;
    m_recvBuffer.clear();
#if defined(Q_OS_WASM)
    m_wsConnected = false;
    const EMSCRIPTEN_WEBSOCKET_T sock = m_websocket;
    m_websocket = 0;
    if (sock > 0) {
        emscripten_websocket_close(sock, 1000, "secondary close");
        emscripten_websocket_delete(sock);
    }
#else
    if (!m_socket)
        return;
    m_socket->disconnectFromHost();
    m_socket.reset();
#endif
}

void TGameserverDualConnection::writeRaw(const QByteArray& payload)
{
    if (!transportConnected() || payload.isEmpty())
        return;
#if defined(Q_OS_WASM)
    emscripten_websocket_send_binary(m_websocket, const_cast<char*>(payload.constData()), payload.size());
#else
    m_socket->write(payload);
#endif
}

void TGameserverDualConnection::sendSecondaryLogin()
{
    // Framed SecondaryLogin (C 0x0B + token). First packet is unencrypted (like primary
    // login); Crystal binds this socket to the primary session and enables shared XTEA
    // afterward. Checksum is ignored on the first packet.
    QByteArray payload;
    payload.append(static_cast<char>(ClientSecondaryLogin));
    appendString(payload, m_token);
    writeRaw(frameModern(payload, 0));
    adoptPrimaryXteaKeys();
}

void TGameserverDualConnection::adoptPrimaryXteaKeys()
{
    // Copy keys from primary ProtocolGame after SecondaryLogin so fan-out /
    // keep-alive frames can be decrypted (Crystal shares one XTEA session).
    const auto primary = g_game.getProtocolGame();
    if (!primary) {
        qCWarning(lcDualConn) << "no primary ProtocolGame; secondary stays keep-alive-only";
        return;
    }
    const auto keys = primary->getXteaKey();
    if (keys.size() != 4) {
        qCWarning(lcDualConn) << "primary XTEA key unavailable";
        return;
    }
    for (int i = 0; i < 4; ++i)
        m_xteaKey[static_cast<size_t>(i)] = keys[static_cast<size_t>(i)];
    m_xteaEnabled = true;
}

void TGameserverDualConnection::deliverSecondaryDecrypted(const quint32 checksum, QByteArray body)
{
    // Crystal OT classic may fan-out duplicate primary frames onto the stability
    // socket. Parsing them would double-apply world state. CIP's load-balanced
    // packet split needs the CipSoft protobuf gameserver (out of scope). Drop
    // the body after a successful decrypt so the secondary remains a keep-alive
    // / SecondaryLogin + ConnectionPingBack peer.
    Q_UNUSED(checksum);
    Q_UNUSED(body);
}

void TGameserverDualConnection::sendConnectionPingBack()
{
    // CIP ConnectionPingBack (C 0x1C). On a sequenced link checksum 0 is the
    // keep-alive at the framing layer (Crystal drops it before XTEA decrypt /
    // parsePacket), so the body need not be XTEA-valid.
    QByteArray payload;
    payload.append(static_cast<char>(ClientConnectionPingBack));
    writeRaw(frameModern(payload, 0));
}

void TGameserverDualConnection::onTransportBytes(const QByteArray& chunk)
{
    m_recvBuffer.append(chunk);
    if (!m_challengeSeen) {
        // Server-sends-first login challenge; consume whatever arrived then SecondaryLogin.
        m_recvBuffer.clear();
        m_challengeSeen = true;
        QUrl url;
#if defined(Q_OS_WASM)
        url.setScheme(QStringLiteral("ws"));
#else
        url.setScheme(QStringLiteral("tcp"));
#endif
        url.setHost(m_host);
        url.setPort(m_port);
        Q_EMIT secondaryConnected(url);
        sendSecondaryLogin();
        sendConnectionPingBack();
        m_pingTimer.start();
        return;
    }
    processRecvBuffer();
}

bool TGameserverDualConnection::decryptXteaInPlace(QByteArray& body) const
{
    return decryptGameXtea1405(m_xteaKey, body);
}

void TGameserverDualConnection::processRecvBuffer()
{
    // 1405 framing: u16le headerSize; remaining = headerSize*8 + 4 (checksum + body).
    while (m_recvBuffer.size() >= 2) {
        const auto headerSize = quint16(uint8_t(m_recvBuffer[0]) | (uint16_t(uint8_t(m_recvBuffer[1])) << 8));
        const int remaining = int(headerSize) * 8 + 4;
        if (remaining <= 0 || remaining > 0xffff) {
            qCWarning(lcDualConn) << "invalid secondary frame headerSize" << headerSize;
            m_recvBuffer.clear();
            return;
        }
        if (m_recvBuffer.size() < 2 + remaining)
            return;

        QByteArray frame = m_recvBuffer.mid(2, remaining);
        m_recvBuffer.remove(0, 2 + remaining);

        if (frame.size() < 4)
            continue;
        const quint32 checksum = quint32(uint8_t(frame[0]))
            | (quint32(uint8_t(frame[1])) << 8)
            | (quint32(uint8_t(frame[2])) << 16)
            | (quint32(uint8_t(frame[3])) << 24);
        QByteArray body = frame.mid(4);

        if (m_xteaEnabled) {
            if (!decryptXteaInPlace(body)) {
                qCWarning(lcDualConn) << "secondary XTEA decrypt failed; checksum" << Qt::hex << checksum;
                continue;
            }
            deliverSecondaryDecrypted(checksum, std::move(body));
        }
    }
}

}
