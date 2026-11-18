#pragma once

#include "input/gameactionptr.h"
#include "sound/soundcontrollerambienceobjectsound.h"
#include "sound/soundcontrollerambiencestream.h"
#include "sound/soundcontrollerbase.h"
#include "sound/soundcontrollercombatsound.h"
#include "sound/soundcontrollermusic.h"

#include <QTimer>

#include <memory>
#include <unordered_map>
#include <variant>

namespace tibia {
namespace sound {

class TGameSessionSoundProvider;

/// A sound trigger (CIP: TGameActionSoundTrigger's value): true/false
/// suspends or resumes the looping world sounds, or an effect to play once.
using TSoundTrigger = std::variant<bool, TSoundEffectRequest>;

/// CIP sound::TSoundController: the client's sound; owns the music, ambience,
/// object and combat controllers.
class TSoundController final : public TSoundControllerBase
{
    Q_OBJECT
public:
    TSoundController(std::shared_ptr<TSoundMetadataManager> metadata, std::shared_ptr<TSoundManager> soundManager,
                     std::weak_ptr<TSoundPlayerManager> playerManager, std::shared_ptr<TSoundConfiguration> configuration);
    ~TSoundController() override;

    void update(int64_t nowMs) override;

    /// The client's controller (CIP hands it to each session's protocol
    /// handler; qtclient's sessions find it here). nullptr without sound.
    static TSoundController* instance();
    static void setInstance(TSoundController* controller);

    /// The game session whose world sounds play (nullptr: none).
    void setGameSessionSoundProvider(std::shared_ptr<TGameSessionSoundProvider> provider);
    /// SoundTrigger type 1: the music template.
    void setMusicTemplate(uint16_t templateId);
    /// SoundTrigger type 0: the ambience stream.
    void setAmbienceStream(uint16_t streamId);
    void handleSoundTrigger(const TSoundTrigger& trigger);

    TSoundControllerMusic& music() const { return *m_music; }
    TSoundControllerAmbienceStream& ambienceStream() const { return *m_ambienceStream; }
    TSoundControllerAmbienceObjectSound& ambienceObjectSound() const { return *m_ambienceObjectSound; }
    TSoundControllerCombatSound& combatSound() const { return *m_combatSound; }

public Q_SLOTS:
    /// SOUND_TRIGGER (CIP handleGameAction; connected to the router's
    /// publishGameActionInternal): the senders already applied their options.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);
    void onEffectSoundsChanged();
    void deferredPlayChangedEffectSounds();
    void onPlayerPositionChanged(const Position& position);
    void onAmbienceObjectStreamCountChanged(uint16_t soundEffectId, size_t count, int nearestDistance);
    void onQueuedSoundsTimerTriggered();

private:
    std::shared_ptr<TGameSessionSoundProvider> m_provider;
    QMetaObject::Connection m_providerConnection;
    std::shared_ptr<TSoundControllerMusic> m_music;
    std::shared_ptr<TSoundControllerAmbienceStream> m_ambienceStream;
    std::shared_ptr<TSoundControllerAmbienceObjectSound> m_ambienceObjectSound;
    std::shared_ptr<TSoundControllerCombatSound> m_combatSound;
    QTimer m_deferredEffectSoundsTimer;
    std::unordered_map<TSoundEffectID, TSoundEffectParameters> m_queuedSounds;
    QTimer m_queuedSoundsTimer;
};

}
}
