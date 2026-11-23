#include "network/gameserverwebsocketdevice.h"

#include <QStringList>
#include <QUrl>
#include <QWebSocket>

#include <emscripten/val.h>

#include <algorithm>
#include <cstring>
#include <utility>

namespace tibia::network {

namespace {

bool isLoopbackOrLan(const QString& host)
{
    if (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1") || host == QLatin1String("::1")
        || host == QLatin1String("0.0.0.0"))
        return true;
    if (host.startsWith(QLatin1String("192.168.")) || host.startsWith(QLatin1String("10.")))
        return true;
    if (host.startsWith(QLatin1String("172."))) {
        const QStringList parts = host.split(QLatin1Char('.'));
        if (parts.size() >= 2) {
            bool ok = false;
            const int octet = parts[1].toInt(&ok);
            if (ok && octet >= 16 && octet <= 31)
                return true;
        }
    }
    return false;
}

/// ws:// for loopback and LAN hosts; wss:// when the page is https (or the
/// port is 443 / a Cloudflare tunnel).
QUrl gameWebSocketUrl(const QString& host, const quint16 port)
{
    QString scheme = QStringLiteral("ws");
    if (!isLoopbackOrLan(host)) {
        const bool forceWss = port == 443 || host.contains(QLatin1String("trycloudflare.com"))
            || host.contains(QLatin1String("cfargotunnel.com"));
        const bool pageIsHttps = emscripten::val::global("location")["protocol"].as<std::string>() == "https:";
        if (forceWss || pageIsHttps)
            scheme = QStringLiteral("wss");
    }
    QUrl url;
    url.setScheme(scheme);
    url.setHost(host);
    url.setPort(port);
    return url;
}

}

TGameserverWebSocketDevice::TGameserverWebSocketDevice(QObject* parent)
    : QIODevice(parent)
{
}

TGameserverWebSocketDevice::~TGameserverWebSocketDevice()
{
    releaseWebSocket();
}

void TGameserverWebSocketDevice::connectToHost(const QString& HostName, const quint16 Port, const QIODevice::OpenMode Mode,
                                               QAbstractSocket::NetworkLayerProtocol)
{
    releaseWebSocket();
    m_readBuffer.clear();
    setOpenMode(Mode);
    m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_webSocket, &QWebSocket::connected, this, &TGameserverWebSocketDevice::connected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &TGameserverWebSocketDevice::disconnected);
    connect(m_webSocket, &QWebSocket::errorOccurred, this, [this](const QAbstractSocket::SocketError SocketError) {
        if (m_webSocket)
            setErrorString(m_webSocket->errorString());
        Q_EMIT errorOccurred(SocketError);
    });
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray& Message) {
        m_readBuffer.append(Message);
        Q_EMIT readyRead();
    });
    m_webSocket->open(gameWebSocketUrl(HostName, Port));
}

void TGameserverWebSocketDevice::disconnectFromHost()
{
    if (m_webSocket)
        m_webSocket->close();
}

void TGameserverWebSocketDevice::abort()
{
    if (m_webSocket)
        m_webSocket->abort();
}

QAbstractSocket::SocketState TGameserverWebSocketDevice::state() const
{
    return m_webSocket ? m_webSocket->state() : QAbstractSocket::UnconnectedState;
}

void TGameserverWebSocketDevice::setSocketOption(QAbstractSocket::SocketOption, const QVariant&)
{
    // The browser owns the WebSocket's TCP connection.
}

bool TGameserverWebSocketDevice::isSequential() const
{
    return true;
}

qint64 TGameserverWebSocketDevice::bytesAvailable() const
{
    return m_readBuffer.size() + QIODevice::bytesAvailable();
}

void TGameserverWebSocketDevice::close()
{
    releaseWebSocket();
    m_readBuffer.clear();
    QIODevice::close();
}

qint64 TGameserverWebSocketDevice::readData(char* data, const qint64 maxSize)
{
    const qint64 count = std::min<qint64>(maxSize, m_readBuffer.size());
    if (count <= 0)
        return 0;
    std::memcpy(data, m_readBuffer.constData(), static_cast<size_t>(count));
    m_readBuffer.remove(0, count);
    return count;
}

qint64 TGameserverWebSocketDevice::writeData(const char* data, const qint64 size)
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState)
        return -1;
    m_webSocket->sendBinaryMessage(QByteArray(data, static_cast<qsizetype>(size)));
    return size;
}

void TGameserverWebSocketDevice::releaseWebSocket()
{
    QWebSocket* webSocket = std::exchange(m_webSocket, nullptr);
    if (!webSocket)
        return;
    // Signals off first: close() reports disconnected() synchronously.
    webSocket->disconnect(this);
    webSocket->close();
    webSocket->deleteLater();
}

}
