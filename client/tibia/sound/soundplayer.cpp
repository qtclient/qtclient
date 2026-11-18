#include "sound/soundplayer.h"

#include <algorithm>

namespace tibia::sound {

float TVolumeModifiers::effectiveVolume() const
{
    const float fadeFactor = static_cast<float>(std::min<uint8_t>(fade, 100)) / 100.0f;
    const float distanceFactor = static_cast<float>(std::min<uint8_t>(distance, 100)) / 100.0f;
    const float volumeFactor = volume / 100.0f;
    const float randomFactor = static_cast<float>(random) / 100.0f;
    return randomFactor * fadeFactor * volumeFactor * distanceFactor * 100.0f;
}

TSoundPlayer::TSoundPlayer(QObject* parent)
    : cip::tibia::sound::TSoundPlayer(parent)
{}

TSoundPlayer::~TSoundPlayer() = default;

bool TSoundPlayer::hasSound(const TPlayableSound& sound) const
{
    return m_sound && m_sound->soundId() == sound.soundId();
}

bool TSoundPlayer::isPlaying() const
{
    return m_waitingForData || m_source.status() == TAudioSource::EStatus::Playing;
}

void TSoundPlayer::setSound(const TPlayableSound& sound)
{
    if (hasSound(sound))
        return;
    m_sound = sound;
    applySound();
}

void TSoundPlayer::applySound()
{
    if (m_sound && m_sound->isReady()) {
        if (!m_waitingForData && m_source.status() == TAudioSource::EStatus::Playing)
            m_source.stop();
        const TPlayableSoundData* data = m_sound->data();
        bool opened = false;
        if (const auto* file = std::get_if<TSoundFile>(data))
            opened = m_source.openFile(file->virtualPath);
        else if (const auto* bytes = std::get_if<QByteArray>(data))
            opened = m_source.openMemory(*bytes);
        else if (const auto* buffer = std::get_if<TSoundBuffer>(data))
            opened = m_source.openBuffer(*buffer);
        if (opened) {
            m_source.setLooping(m_looping);
            updateVolume();
            m_source.setPitch(m_pitch);
        }
        if (!m_waitingForData)
            return;
        if (m_sound->isLoading())
            return;
        m_source.play();
        m_waitingForData = false;
        return;
    }
    if (m_waitingForData || m_source.status() == TAudioSource::EStatus::Playing)
        m_source.stop();
    m_source.close();
}

void TSoundPlayer::play()
{
    if (m_sound && m_sound->isLoading()) {
        m_waitingForData = true;
        return;
    }
    applySound();
    m_source.play();
}

void TSoundPlayer::stop()
{
    m_waitingForData = false;
    m_source.stop();
}

void TSoundPlayer::reset()
{
    m_waitingForData = false;
    m_source.stop();
    if (m_pitch != 1.0f) {
        m_pitch = 1.0f;
        m_source.setPitch(m_pitch);
    }
    m_volumeModifiers = TVolumeModifiers{};
    updateVolume();
    m_source.close();
    if (m_looping) {
        m_looping = false;
        m_source.setLooping(false);
    }
    m_sound.reset();
}

void TSoundPlayer::setVolumeModifiers(const TVolumeModifiers& modifiers)
{
    if (m_volumeModifiers == modifiers)
        return;
    m_volumeModifiers = modifiers;
    updateVolume();
}

void TSoundPlayer::updateVolume()
{
    const float volume = m_volumeModifiers.effectiveVolume();
    if (m_source.volume() != volume)
        m_source.setVolume(volume);
}

void TSoundPlayer::setLooping(const bool looping)
{
    if (m_looping == looping)
        return;
    m_looping = looping;
    m_source.setLooping(looping);
}

void TSoundPlayer::setPitch(const float pitch)
{
    if (m_pitch == pitch)
        return;
    m_pitch = pitch;
    m_source.setPitch(pitch);
}

int64_t TSoundPlayer::playingOffsetMs() const
{
    return m_source.playingOffsetMs();
}

void TSoundPlayer::setPlayingOffsetMs(const int64_t offsetMs)
{
    m_source.setPlayingOffsetMs(offsetMs);
}

int64_t TSoundPlayer::durationMs() const
{
    return m_source.durationMs();
}

}
