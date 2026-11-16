#include "rpcserviceconnection.h"

#include <QDataStream>
#include <QDebug>
#include <QJsonParseError>

#include <algorithm>

namespace tibia::shared {

TRPCServiceConnection::TRPCServiceConnection(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    connect(m_socket, &QAbstractSocket::disconnected, this, [this] { Q_EMIT disconnected(this); });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &TRPCServiceConnection::onSocketError);
    connect(m_socket, &QAbstractSocket::stateChanged, this, &TRPCServiceConnection::onSocketStateChanged);
    connect(m_socket, &QIODevice::readyRead, this, &TRPCServiceConnection::onDataReady);
}

TRPCServiceConnection::~TRPCServiceConnection()
{
    close();
}

void TRPCServiceConnection::close()
{
    if (!m_socket)
        return;
    m_socket->setParent(nullptr);
    m_socket->deleteLater();
    m_socket = nullptr;
}

void TRPCServiceConnection::send(const QJsonDocument& message)
{
    const QByteArray json = message.toJson(QJsonDocument::Compact);
    QDataStream stream(m_socket);
    stream << json;
}

int TRPCServiceConnection::readState() const
{
    return m_readState;
}

void TRPCServiceConnection::onSocketError(const QAbstractSocket::SocketError socketError)
{
    Q_EMIT connectionError(this, socketError, m_socket->errorString());
}

void TRPCServiceConnection::onSocketStateChanged()
{
}

void TRPCServiceConnection::onDataReady()
{
    bool dataRead = false;
    while (m_socket && m_socket->bytesAvailable() > 0) {
        if (m_readState == 0) {
            if (m_socket->bytesAvailable() < 4)
                break;
            const QByteArray length = m_socket->read(4);
            QDataStream stream(length);
            stream >> m_messageSize;
            m_readState = 1;
            dataRead = true;
        }
        if (m_readState == 1) {
            if (m_socket->bytesAvailable() > 0) {
                const qint64 missing = qint64(m_messageSize) - m_message.size();
                const qint64 count = std::min(missing, m_socket->bytesAvailable());
                if (count > 0) {
                    m_message.append(m_socket->read(count));
                    dataRead = true;
                }
            }
            if (m_message.size() < qsizetype(m_messageSize))
                break;
            m_readState = 2;
        }
        if (m_readState == 2) {
            QJsonParseError error { -1, QJsonParseError::NoError };
            const QJsonDocument message = QJsonDocument::fromJson(m_message, &error);
            if (error.error != QJsonParseError::NoError) {
                qCritical() << "Json document was malformed, could not extract message";
                close();
                break;
            }
            Q_EMIT receivedMessage(this, message);
            m_readState = 0;
            m_messageSize = 0;
            m_message.clear();
        }
    }
    if (dataRead)
        Q_EMIT newDataReceived();
}

}
