#pragma once

// Host audio backend. CipSoft plays sound through SFML (sf::SoundBuffer,
// sf::Sound, sf::Music, sf::PlaybackDevice); qtclient plays the same model
// through miniaudio. The sound classes use only this interface.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tibia {
namespace sound {

/// Decoded interleaved samples (CIP: TRawSoundSamples / sf::SoundBuffer data).
struct TRawSoundSamples
{
    std::vector<float> samples;
    uint32_t channelCount = 0;
    uint32_t sampleRate = 0;

    bool empty() const { return samples.empty() || channelCount == 0 || sampleRate == 0; }
};

/// A decoded sound shared by every source that plays it (CIP: sf::SoundBuffer).
struct TSoundBuffer
{
    std::shared_ptr<const TRawSoundSamples> samples;

    bool empty() const { return !samples || samples->empty(); }
};

/// Decodes an Ogg Vorbis file in memory (CIP: sf::SoundBuffer::loadFromMemory).
std::optional<TRawSoundSamples> decodeSoundData(const QByteArray& data);

/// One playing voice (CIP: sf::Sound for a buffer, sf::Music for a stream).
class TAudioSource final
{
public:
    enum class EStatus : int { Stopped = 0, Paused = 1, Playing = 2 };

    TAudioSource();
    ~TAudioSource();
    TAudioSource(const TAudioSource&) = delete;
    TAudioSource& operator=(const TAudioSource&) = delete;

    /// sf::Sound(buffer).
    bool openBuffer(const TSoundBuffer& buffer);
    /// sf::Music::openFromMemory: decodes while it plays.
    bool openMemory(const QByteArray& data);
    /// sf::Music::openFromFile (a path the data file system resolves).
    bool openFile(const std::string& virtualPath);
    bool isOpen() const;

    void play();
    void pause();
    void stop();
    EStatus status() const;

    /// 0..100 like SFML.
    float volume() const { return m_volume; }
    void setVolume(float volume);
    float pitch() const { return m_pitch; }
    void setPitch(float pitch);
    bool looping() const { return m_looping; }
    void setLooping(bool looping);
    int64_t playingOffsetMs() const;
    void setPlayingOffsetMs(int64_t offsetMs);
    int64_t durationMs() const;

    /// Closes the data; the source is silent until opened again.
    void close();

    struct Impl;

private:

    std::unique_ptr<Impl> m_impl;
    float m_volume = 100.0f;
    float m_pitch = 1.0f;
    bool m_looping = false;
};

/// The output device (CIP: sf::PlaybackDevice).
namespace playbackdevice {

/// Initialises the engine on the default device; false when there is none.
bool initialize();
bool isInitialized();
QStringList availableDevices();
std::optional<QString> defaultDevice();
std::optional<QString> device();
/// Moves the engine and every open source to `name`.
bool setDevice(const QString& name);
/// Host: stops/starts the device without closing it (a muted master volume
/// releases the platform audio session; browsers show the tab speaker otherwise).
void setSuspended(bool suspended);

}

}
}
