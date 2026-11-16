#include "launcherutils.h"

#include <QDebug>
#include <QHostAddress>
#ifndef __EMSCRIPTEN__
#include <QNetworkInterface>
#endif
#include <QTcpSocket>

namespace tibia::shared {

namespace {

bool s_hasLocalIPv4Checked = false;
bool s_hasLocalIPv4 = false;

}

bool TLauncherUtils::hasLocalIPv4()
{
    if (s_hasLocalIPv4Checked)
        return s_hasLocalIPv4;
    s_hasLocalIPv4 = false;
#ifndef __EMSCRIPTEN__
    // Host: the browser has no local interfaces or raw sockets to probe.
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address == QHostAddress(QHostAddress::LocalHost))
            continue;
        qInfo() << "ipv4 interface found";
        bool connected = false;
        {
            QTcpSocket socket;
            socket.connectToHost(QStringLiteral("8.8.8.8"), 53, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
            connected = socket.waitForConnected(750);
            if (connected)
                socket.disconnectFromHost();
        }
        if (!connected) {
            QTcpSocket socket;
            socket.connectToHost(QStringLiteral("1.1.1.1"), 80, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
            connected = socket.waitForConnected(750);
            if (connected)
                socket.disconnectFromHost();
        }
        if (connected) {
            qInfo() << "ipv4 connection succeeded";
            s_hasLocalIPv4 = true;
        }
        break;
    }
#endif
    qInfo() << "use ipv4: " << (s_hasLocalIPv4 ? "true" : "false");
    s_hasLocalIPv4Checked = true;
    return s_hasLocalIPv4;
}

bool TLauncherUtils::canConnect(const QString& host, const quint16 port, const int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, port, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
    const bool connected = socket.waitForConnected(timeoutMs);
    if (connected)
        socket.disconnectFromHost();
    return connected;
}

}
