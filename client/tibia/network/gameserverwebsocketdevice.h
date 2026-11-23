#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QVariant>

class QWebSocket;

namespace tibia {
namespace network {

/// Browser stand-in for the game server's QTcpSocket (browsers have no TCP):
/// a WebSocket to the game server's WebSocket port, read and written as the
/// same byte stream. Binary messages are appended to the read buffer; every
/// write goes out as one binary message. Offers the part of QAbstractSocket's
/// interface that TGameserverTCPConnection and TGameserverNetworkPacketConnection use.
class TGameserverWebSocketDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit TGameserverWebSocketDevice(QObject* parent = nullptr);
    ~TGameserverWebSocketDevice() override;

    void connectToHost(const QString& HostName, quint16 Port, QIODevice::OpenMode Mode = QIODevice::ReadWrite,
                       QAbstractSocket::NetworkLayerProtocol Protocol = QAbstractSocket::AnyIPProtocol);
    void disconnectFromHost();
    void abort();
    QAbstractSocket::SocketState state() const;
    void setSocketOption(QAbstractSocket::SocketOption Option, const QVariant& Value);

    bool isSequential() const override;
    qint64 bytesAvailable() const override;
    void close() override;

Q_SIGNALS:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError SocketError);

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 size) override;

private:
    void releaseWebSocket();

    QWebSocket* m_webSocket = nullptr;
    QByteArray m_readBuffer;
};

}
}
