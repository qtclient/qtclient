#include "gameserverdualconnection.h"

#include "gameserverconnection.h"
#include "app/clientoptions.h"
#include "client/gamecore.h"

#include <tibia/game/gamesession.h>
#include "network/gameserverpacketprocessor.h"

#include <QTimer>
#include <QUrl>

#include <tibia/game/gamenotify.h>

namespace tibia::network {

namespace {

constexpr quint8 ClientSecondaryLogin = 0x0B;
constexpr quint8 ClientConnectionPingBack = 0x1C;

using client::gamecore::argString;

/// Little-endian u16 length prefix + UTF-8.
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

}

TGameserverDualConnection::TGameserverDualConnection(QObject* parent)
    : cip::tibia::network::TGameserverDualConnection(parent)
{
    m_enabled = optimizationEnabled();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("connectionStabilityOptimizationEnabled"))
            onOptimizeConnectionStabilityOptionChanged();
    });

    m_readyHook = tibia::game::SoftHook(
        "onReadyForSecondaryConnection", [this](const tibia::game::TGameNotify::Args& args) {
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
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void TGameserverDualConnection::openSecondaryIfNeeded()
{
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
        return;

    m_challengeSeen = false;
    m_xteaEnabled = false;
    m_recvBuffer.clear();

    const auto onDisconnected = [this] {
        m_pingTimer.stop();
        m_challengeSeen = false;
        m_xteaEnabled = false;
        m_recvBuffer.clear();
        Q_EMIT disconnected();
    };
#if defined(Q_OS_WASM)
    // Browsers have no TCP: the same peer over the game WebSocket port.
    m_socket = std::make_unique<QWebSocket>(QString(), QWebSocketProtocol::VersionLatest);
    connect(m_socket.get(), &QWebSocket::binaryMessageReceived, this,
            [this](const QByteArray& chunk) { onTransportBytes(chunk); });
    connect(m_socket.get(), &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        Q_EMIT error(tr("secondary WebSocket error"));
    });
    connect(m_socket.get(), &QWebSocket::disconnected, this, onDisconnected);
    m_socket->open(gameWebSocketUrl(m_host, static_cast<uint16_t>(m_port)));
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
    connect(m_socket.get(), &QTcpSocket::disconnected, this, onDisconnected);
    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));
#endif
}

void TGameserverDualConnection::closeSecondary()
{
    m_pingTimer.stop();
    m_challengeSeen = false;
    m_xteaEnabled = false;
    m_recvBuffer.clear();
    if (!m_socket)
        return;
    // Signals off first: closing reports disconnected() synchronously.
    m_socket->disconnect(this);
#if defined(Q_OS_WASM)
    m_socket->close();
#else
    m_socket->disconnectFromHost();
#endif
    m_socket.reset();
}

void TGameserverDualConnection::writeRaw(const QByteArray& payload)
{
    if (!transportConnected() || payload.isEmpty())
        return;
#if defined(Q_OS_WASM)
    m_socket->sendBinaryMessage(payload);
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
    // Copy keys from the primary connection after SecondaryLogin so fan-out /
    // keep-alive frames can be decrypted (Crystal shares one XTEA session).
    const auto* primary = TGameserverNetworkPacketProcessor::live();
    if (!primary) {
        return;
    }
    m_xteaKey = primary->xteaKey();
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
                continue;
            }
            deliverSecondaryDecrypted(checksum, std::move(body));
        }
    }
}

}
