#pragma once

#include <QByteArray>
#include <QObject>

#include <memory>

class ProtocolGame;

namespace tibia::protobuf::protocol {
class GameserverMessage;
class GameclientMessage;
}

namespace tibia::protocol {

/// CIP-shaped sink: `TProtocolMessageQueue` → classic `ProtocolGame` parsers.
/// Temporary until each opcode has a native `*ProtocolMessageHandler` slot.
/// IdleOT / product mods must not hang here — they use ExtendedOpcodeRouter.
class TClassicProtocolBridgeHandler final : public QObject
{
    Q_OBJECT
public:
    explicit TClassicProtocolBridgeHandler(QObject* parent = nullptr);

    void setProtocolGame(const std::shared_ptr<ProtocolGame>& protocol);

public Q_SLOTS:
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
    void onClientMessageEnqueued(int opcode, const QByteArray& classicFrame);

private:
    std::weak_ptr<ProtocolGame> m_protocol;
};

void ownClassicProtocolBridge(QObject* parent);

} // namespace tibia::protocol
