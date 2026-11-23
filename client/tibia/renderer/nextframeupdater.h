#pragma once

#include <QtGlobal>

#include <memory>

namespace tibia {
namespace analysers {
class TDamageInputStorage;
class TGainWasteStorage;
class THuntingSessionStorage;
class TImpactStorage;
class TProgressStorage;
}
namespace chat {
class TTextStorage;
}
namespace cyclopedia {
class TItemTrackingStorage;
}
namespace effects {
class TEffectStorage;
}
namespace game {
class TCooldownStorage;
}
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
namespace sound {
class TSoundStorage;
}
namespace worldmap {
class TAmbientLightStorage;
class TWorldMapStorage;
}
}

namespace tibia {
namespace renderer {

/// Advances everything that changes with time to the moment the next frame
/// is shown (CIP: TNextFrameUpdater). The game session creates it with its
/// storages and calls update() once per frame with the predicted timestamp;
/// the storages advance in a fixed order.
class TNextFrameUpdater
{
public:
    TNextFrameUpdater(std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage, std::shared_ptr<effects::TEffectStorage> effectStorage,
                      std::shared_ptr<chat::TTextStorage> textStorage, std::shared_ptr<game::TCooldownStorage> cooldownStorage,
                      std::shared_ptr<worldmap::TAmbientLightStorage> ambientLightStorage,
                      std::shared_ptr<cyclopedia::TItemTrackingStorage> itemTrackingStorage,
                      std::shared_ptr<analysers::TGainWasteStorage> gainWasteStorage,
                      std::shared_ptr<analysers::TImpactStorage> impactStorage,
                      std::shared_ptr<analysers::TDamageInputStorage> damageInputStorage,
                      std::shared_ptr<analysers::TProgressStorage> progressStorage,
                      std::shared_ptr<analysers::THuntingSessionStorage> huntingSessionStorage,
                      std::shared_ptr<sound::TSoundStorage> soundStorage);
    virtual ~TNextFrameUpdater() = default;

    /// Host: the world map render provider, whose frame is built after the
    /// storages advanced.
    void setWorldMapRenderProvider(std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider);

    /// Advances the world map's animations, the effects, the texts, the
    /// cooldowns, the ambient light, the analysers and the queued sounds to
    /// `nextFrameTimestamp`.
    virtual void update(const qint64& nextFrameTimestamp);

private:
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<effects::TEffectStorage> m_effectStorage;
    std::shared_ptr<chat::TTextStorage> m_textStorage;
    std::shared_ptr<game::TCooldownStorage> m_cooldownStorage;
    std::shared_ptr<worldmap::TAmbientLightStorage> m_ambientLightStorage;
    std::shared_ptr<cyclopedia::TItemTrackingStorage> m_itemTrackingStorage;
    std::shared_ptr<analysers::TGainWasteStorage> m_gainWasteStorage;
    std::shared_ptr<analysers::TImpactStorage> m_impactStorage;
    std::shared_ptr<analysers::TDamageInputStorage> m_damageInputStorage;
    std::shared_ptr<analysers::TProgressStorage> m_progressStorage;
    std::shared_ptr<analysers::THuntingSessionStorage> m_huntingSessionStorage;
    std::shared_ptr<sound::TSoundStorage> m_soundStorage;
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
};

}
}
