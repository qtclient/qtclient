#pragma once

#include <cip_gen.h>

#include <QByteArray>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageGraphicalEffects;
class GameserverMessageRemoveGraphicalEffect;
}

namespace tibia::shared {
class IClockService;
}
namespace tibia::sound {
class TSoundStorage;
}
namespace tibia::worldmap {
class TWorldMapStorage;
}

namespace tibia::effects {

class TEffectStorage;

/// Places the server's graphical effects, missiles and effect sounds, and
/// removes graphical effects the server takes back.
class TEffectProtocolMessageHandler final : public cip::tibia::effects::TEffectProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler keeps the session's storages it fills and the session's
    /// clock. Host: the appearance instance factory is static in qtclient, and
    /// the effect opacities come from client::magicEffectOpacity.
    TEffectProtocolMessageHandler(std::shared_ptr<TEffectStorage> effectStorage,
                                  std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                                  std::shared_ptr<sound::TSoundStorage> soundStorage,
                                  std::shared_ptr<shared::IClockService> clockService);

    void handleGraphicalEffectsMessage(const protobuf::protocol::GameserverMessageGraphicalEffects& message);
    void handleRemoveGraphicalEffectMessage(const protobuf::protocol::GameserverMessageRemoveGraphicalEffect& message);

private:
    std::shared_ptr<TEffectStorage> m_effectStorage;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<sound::TSoundStorage> m_soundStorage;
    std::shared_ptr<shared::IClockService> m_clockService;
};

}
