#pragma once

// Host: an extension holding CipSoft's ambience channel. Not a CipSoft class;
// a thin adapter onto TSoundController's ambience stream and single shots,
// plus a bed and one-shots from sound files, all at the ambience volume. The
// host ABI's sound table (QtClientSoundApiV1) is this class. Without a pack
// nothing holds the channel and the server's ambience streams play as in CipSoft.

#include "qtclient/abi/sound_api.h"
#include "sound/audiobackend.h"

#include <QPointer>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace tibia {
namespace sound {

class TSoundController;

class TAmbienceHold final
{
public:
    static TAmbienceHold& instance();

    /// The game client's sound controller the hold plays through (null: no sound).
    void setSoundController(TSoundController* controller);

    /// The place's ambience stream (the server's), 0 for none.
    uint16_t currentAmbienceId() const;
    bool held() const { return m_held; }
    /// The extension owns the ambience: the place stream fades out and later
    /// stream triggers wait until the hold ends.
    void begin();
    /// Ends the hold: the bed stops, the deferred stream (else
    /// `fallbackRestoreId`) plays.
    void end(uint16_t fallbackRestoreId);
    /// Plays a CipSoft ambience stream as the held bed.
    bool playStream(uint16_t ambienceStreamId);
    /// Plays a sound file as the held bed.
    bool playFile(const std::string& virtualPath, bool loop = true);
    /// Multiplier of the bed's volume (resets to 1 when the hold ends).
    void setGain(float gain);
    float gain() const { return m_gain; }
    /// A numeric sound effect once at `gain` × the ambience volume.
    bool playEffect(uint16_t soundEffectId, float gain = 1.0f);
    /// A sound file once at `gain` × the ambience volume.
    bool playOneshot(const std::string& virtualPath, float gain = 1.0f);

    /// Protocol ambience trigger while held: kept for end(). False when nothing holds the channel.
    bool deferAmbienceStream(uint16_t streamId);
    /// Option change: the bed follows the ambience volume.
    void updateVolume();

private:
    TAmbienceHold();
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

/// The host ABI's sound table over TAmbienceHold.
const QtClientSoundApiV1* soundApiV1();

}
}
