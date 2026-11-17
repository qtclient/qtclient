#pragma once

#include "sound/audiobackend.h"
#include "sound/soundmanager.h"

#include <cip_gen.h>

#include <cstdint>
#include <optional>

namespace tibia {
namespace sound {

/// The factors of a player's volume, in percent (CIP: TVolumeModifiers).
struct TVolumeModifiers
{
    uint8_t fade = 100;
    uint8_t distance = 100;
    float volume = 100.0f;
    uint8_t random = 100;

    bool operator==(const TVolumeModifiers&) const = default;
    /// fade × distance × volume × random, in percent.
    float effectiveVolume() const;
};

/// CIP sound::TSoundPlayer: one voice of the player pool.
class TSoundPlayer final : public cip::tibia::sound::TSoundPlayer
{
    Q_OBJECT
public:
    explicit TSoundPlayer(QObject* parent = nullptr);
    ~TSoundPlayer() override;

    /// v12: plays the same sound as `sound`.
    bool hasSound(const TPlayableSound& sound) const;
    /// Playing, or waiting for its data to start playing.
    bool isPlaying() const;
    bool isWaitingForData() const { return m_waitingForData; }
    /// Assigns the sound (no change when it is already the player's sound).
    void setSound(const TPlayableSound& sound);
    const std::optional<TPlayableSound>& sound() const { return m_sound; }
    /// Opens the sound's data in the source; starts it when it was waiting.
    void applySound();
    /// Starts the sound, or waits until its data is converted.
    void play();
    void stop();
    /// Stops and forgets the sound; modifiers, pitch and looping return to defaults.
    void reset();

    const TVolumeModifiers& volumeModifiers() const { return m_volumeModifiers; }
    void setVolumeModifiers(const TVolumeModifiers& modifiers);
    void updateVolume();
    bool looping() const { return m_looping; }
    void setLooping(bool looping);
    float pitch() const { return m_pitch; }
    void setPitch(float pitch);
    int64_t playingOffsetMs() const;
    void setPlayingOffsetMs(int64_t offsetMs);
    int64_t durationMs() const;

private:
    TAudioSource m_source;
    std::optional<TPlayableSound> m_sound;
    bool m_waitingForData = false;
    bool m_looping = false;
    TVolumeModifiers m_volumeModifiers;
    float m_pitch = 1.0f;
};

}
}
