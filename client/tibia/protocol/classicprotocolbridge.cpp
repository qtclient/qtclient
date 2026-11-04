#include "protocol/classicprotocolbridge.h"

#include "protocol/protocolciptypes.h"

#include "client/protocolgame.h"
#include "framework/net/inputmessage.h"
#include "framework/net/protocol.h"

#include <QLoggingCategory>

#include <tibia/game/gamesession.h>

namespace tibia::protocol {

namespace {
Q_LOGGING_CATEGORY(lcBridge, "tibia.protocol.bridge")

TClassicProtocolBridgeHandler* g_bridge = nullptr;
} // namespace

TClassicProtocolBridgeHandler::TClassicProtocolBridgeHandler(QObject* parent)
    : QObject(parent)
{
}

void TClassicProtocolBridgeHandler::setProtocolGame(const std::shared_ptr<ProtocolGame>& protocol)
{
    m_protocol = protocol;
}

void TClassicProtocolBridgeHandler::onServerMessageEnqueued(int opcode, const QByteArray& classicFrame,
                                                           bool applyClassic)
{
    if (!applyClassic || classicFrame.isEmpty())
        return;
    auto protocol = m_protocol.lock();
    if (!protocol)
        protocol = g_game.getProtocolGame();
    if (!protocol) {
        qCDebug(lcBridge) << "drop server frame opcode" << Qt::hex << opcode << "no ProtocolGame";
        return;
    }
    protocol->applyClassicServerFrame(classicFrame);
}

void TClassicProtocolBridgeHandler::onClientMessageEnqueued(int /*opcode*/, const QByteArray& classicFrame)
{
    // Wire sink: classic bytes already prepared by the client translator.
    auto protocol = m_protocol.lock();
    if (!protocol)
        protocol = g_game.getProtocolGame();
    if (!protocol || classicFrame.isEmpty())
        return;
    protocol->writeClassicClientFrame(classicFrame);
}

void ownClassicProtocolBridge(QObject* parent)
{
    if (g_bridge)
        return;
    g_bridge = new TClassicProtocolBridgeHandler(parent);
    auto* queue = protocolMessageQueue();
    if (!queue)
        return;
    QObject::connect(queue, &TProtocolMessageQueue::serverMessageEnqueued, g_bridge,
                     &TClassicProtocolBridgeHandler::onServerMessageEnqueued);
    QObject::connect(queue, &TProtocolMessageQueue::clientMessageEnqueued, g_bridge,
                     &TClassicProtocolBridgeHandler::onClientMessageEnqueued);
}

} // namespace tibia::protocol
