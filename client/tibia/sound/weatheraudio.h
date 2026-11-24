#pragma once

// Host: the pack's weather audio (IdleOT). Not a CipSoft class; a thin
// adapter onto TSoundController's ambience stream and single shots, plus a
// bed and one-shots from pack sound files.

#include "sound/audiobackend.h"

#include <QPointer>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace tibia {
namespace sound {

class TSoundController;

class TWeatherAudio final
{
public:
    static TWeatherAudio& instance();

    /// The game client's sound controller the weather plays through (null:
    /// no sound).
    void setSoundController(TSoundController* controller);

    /// The place's ambience stream (the server's), 0 for none.
    uint16_t currentAmbienceId() const;
    bool ambienceHeld() const { return m_held; }
    /// Weather owns the ambience: the place stream fades out and later
    /// stream triggers wait until the hold ends.
    void beginAmbienceHold();
    /// Ends the hold: the weather bed stops, the deferred stream (else
    /// `fallbackRestoreId`) plays.
    void endAmbienceHold(uint16_t fallbackRestoreId);
    /// Plays a CipSoft ambience stream as the held bed.
    bool playHeldAmbience(uint16_t ambienceStreamId, bool loop = true);
    /// Plays a pack sound file as the held bed.
    bool playHeldAmbiencePath(const std::string& virtualPath, bool loop = true);
    /// Multiplier of the pack bed's volume (resets to 1 when the hold ends).
    void setAmbienceVoiceGain(float gain);
    float ambienceVoiceGain() const { return m_gain; }
    /// A numeric sound effect once at `gain` × the ambience volume.
    bool playEffect(uint16_t soundEffectId, float gain = 1.0f, float pan = 0.0f);
    /// A pack sound file once at `gain` × the ambience volume.
    bool playAmbienceOneshot(const std::string& virtualPath, float gain = 1.0f, float pan = 0.0f);

    /// Protocol ambience trigger while held: kept for endAmbienceHold.
    bool deferAmbienceStream(uint16_t streamId);
    /// Option change: the pack bed follows the ambience volume.
    void updateVolume();

private:
    TWeatherAudio();
    float ambienceVolume() const;

    QPointer<TSoundController> m_controller;
    bool m_held = false;
    bool m_hasDeferred = false;
    uint16_t m_deferredStreamId = 0;
    float m_gain = 1.0f;
    std::unique_ptr<TAudioSource> m_bed;
    std::array<std::unique_ptr<TAudioSource>, 8> m_oneshots;
    size_t m_nextOneshot = 0;
};

}
}
