#pragma once

#include "sound/soundplayermanager.h"

#include <cip_gen.h>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace sound {

/// CIP sound::TActiveSoundPlayer: a long sound (music, an ambience loop)
/// that cross-fades from the playing sound to the next one.
class TActiveSoundPlayer final : public cip::tibia::sound::TActiveSoundPlayer
{
    Q_OBJECT
public:
    /// A pool player with its fade.
    struct TFadingSoundPlayer
    {
        explicit TFadingSoundPlayer(TSoundPlayerToken token);

        TSoundPlayer* player() const { return token.player(); }
        /// Turns the fade to `direction` (0 in, 1 out), keeping the level.
        void setDirection(int direction, int64_t nowMs);
        void setFade(int direction, int64_t durationMs, uint8_t steps);
        bool isFinished(int64_t nowMs) const { return durationMs + startMs < nowMs; }
        /// The player's fade modifier at `nowMs`.
        void updateFade(int64_t nowMs);

        TSoundPlayerToken token;
        int direction = 0;
        int64_t durationMs = 0;
        uint8_t steps = 100;
        int64_t startMs = 0;
    };

    explicit TActiveSoundPlayer(std::weak_ptr<TSoundPlayerManager> playerManager, QObject* parent = nullptr);
    ~TActiveSoundPlayer() override;

    void setFade(int fadeType, int64_t durationMs, uint8_t steps);
    int volume() const { return static_cast<int>(m_volume); }
    void setVolume(float volume);
    float pitch() const { return m_pitch; }
    void setPitch(float pitch);
    uint8_t distanceModifier() const { return m_distanceModifier; }
    void setDistanceModifier(uint8_t distanceModifier);
    bool looping() const { return m_looping; }
    void setLooping(bool looping);

    /// Plays `sound`, fading out the playing one.
    void play(const TPlayableSound& sound);
    /// The sound that starts once the playing one faded out.
    void setNextSound(const TPlayableSound& sound);
    /// Fades the playing sound out.
    void fadeOut();
    /// Fades out and ends every fade now.
    void stopNow();
    /// Stops every player and restores volume and distance.
    void reset();
    void update(int64_t nowMs);

    /// Something plays or fades.
    bool isPlaying() const;
    TSoundEffectID currentSoundId() const;
    /// Fade level of the playing sound, else of the last fading one.
    uint8_t currentFadeLevel() const;
    uint8_t lastFadingLevel() const;
    TSoundEffectID lastFadingSoundId() const;
    int64_t playingOffsetMs() const;
    int64_t durationMs() const;
    void setPlayingOffsetMs(int64_t offsetMs);

private:
    void startPlaying();
    void updateVolume();
    void updateDistanceModifier();
    void updatePitch();
    void updateLooping();
    std::optional<TFadingSoundPlayer> acquirePlayer();

    std::weak_ptr<TSoundPlayerManager> m_playerManager;
    std::optional<TFadingSoundPlayer> m_currentPlayer;
    std::vector<TFadingSoundPlayer> m_fadingPlayers;
    int m_fadeType = 0;
    int64_t m_fadeDurationMs = 0;
    uint8_t m_fadeSteps = 20;
    bool m_finishedNotified = false;
    std::optional<TPlayableSound> m_nextSound;
    float m_pitch = 1.0f;
    uint8_t m_distanceModifier = 100;
    float m_volume = 100.0f;
    int64_t m_nowMs = 0;
    bool m_looping = false;
};

}
}
