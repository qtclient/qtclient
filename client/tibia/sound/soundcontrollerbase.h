#pragma once

#include "sound/activesoundplayer.h"
#include "sound/soundconfiguration.h"
#include "sound/soundmanager.h"
#include "sound/soundmetadatamanager.h"
#include "sound/soundplayermanager.h"

#include <cip_gen.h>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace sound {

/// A numeric sound effect to play (CIP: the sound trigger's effect request).
struct TSoundEffectRequest
{
    TSoundEffectID effectId = 0;
    std::optional<TChatSoundContext> chat;
    /// 0 none, 1 own, 2 other players, 3 creatures.
    int source = 0;
};

/// A numeric sound effect resolved to a sound (CIP: TSoundEffectParameters).
struct TSoundEffectParameters
{
    TSoundEffectID soundId = 0;
    TVolumeModifiers volumeModifiers;
    float pitch = 1.0f;
};

/// CIP sound::TSoundControllerBase: what every sound controller shares.
class TSoundControllerBase : public cip::tibia::sound::TSoundControllerBase
{
    Q_OBJECT
public:
    TSoundControllerBase(std::shared_ptr<TSoundMetadataManager> metadata, std::shared_ptr<TSoundManager> soundManager,
                         std::weak_ptr<TSoundPlayerManager> playerManager,
                         std::shared_ptr<TSoundConfiguration> configuration);
    ~TSoundControllerBase() override;

    /// v12: advances the players to `nowMs`.
    virtual void update(int64_t nowMs);
    /// v13: no game runs (login screen); looping world sounds stop.
    virtual void setSuspended(bool suspended) { m_suspended = suspended; }
    bool isSuspended() const { return m_suspended; }

    const std::shared_ptr<TSoundMetadataManager>& metadata() const { return m_metadata; }
    const std::shared_ptr<TSoundManager>& soundManager() const { return m_soundManager; }
    const std::shared_ptr<TSoundConfiguration>& configuration() const { return m_configuration; }
    int64_t now() const { return m_nowMs; }

    /// The sound, pitch and volume for a numeric sound effect.
    TSoundEffectParameters effectParameters(const TSoundEffectRequest& request) const;
    /// Plays `parameters` once on a pool player.
    std::optional<TSoundPlayerToken> playSingleShot(const TSoundEffectParameters& parameters);
    /// Keeps a single shot's player until it finished.
    void keepSingleShot(TSoundPlayerToken token);
    size_t freePlayerCount() const;

protected Q_SLOTS:
    /// v14
    virtual void onSoundConfigurationChanged() {}

protected:
    TActiveSoundPlayer* createActivePlayer();
    /// The controller updates `player` until it stopped, then deletes it.
    void addActivePlayer(TActiveSoundPlayer* player);
    std::vector<TSoundPlayerToken>& singleShots() { return m_singleShots; }
    const std::vector<TActiveSoundPlayer*>& activePlayers() const { return m_activePlayers; }

private:
    std::optional<TSoundPlayerToken> playSingleShot(float pitch, TSoundEffectID soundId,
                                                    const TVolumeModifiers& modifiers);

    std::shared_ptr<TSoundMetadataManager> m_metadata;
    std::shared_ptr<TSoundManager> m_soundManager;
    std::weak_ptr<TSoundPlayerManager> m_playerManager;
    std::shared_ptr<TSoundConfiguration> m_configuration;
    int64_t m_nowMs = 0;
    std::vector<TActiveSoundPlayer*> m_activePlayers;
    std::vector<TSoundPlayerToken> m_singleShots;
    bool m_suspended = false;
};

/// A uniform random value in [min, max].
double randomBetween(double min, double max);
int64_t randomBetween(int64_t min, int64_t max);

}
}
