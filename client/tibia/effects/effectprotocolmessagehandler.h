#pragma once

#include <cip_gen.h>

#include <QByteArray>

namespace tibia::protobuf::protocol {
class GameserverMessageGraphicalEffects;
class GameserverMessageRemoveGraphicalEffect;
}

namespace tibia::effects {

/// Places the server's graphical effects, missiles and effect sounds, and
/// removes graphical effects the server takes back.
class TEffectProtocolMessageHandler final : public cip::tibia::effects::TEffectProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::effects::TEffectProtocolMessageHandler::TEffectProtocolMessageHandler;

    void handleGraphicalEffectsMessage(const protobuf::protocol::GameserverMessageGraphicalEffects& message);
    void handleRemoveGraphicalEffectMessage(const protobuf::protocol::GameserverMessageRemoveGraphicalEffect& message);

    /// Queue wire: the structured effect messages, which skip the classic bridge.
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

}
