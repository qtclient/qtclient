#include "sound/soundstorage.h"

#include <algorithm>

namespace tibia::sound {

TSoundStorage::TSoundStorage(const int maxSoundCount)
    : m_maxSoundCount(maxSoundCount)
{}

TSoundStorage::~TSoundStorage() = default;

void TSoundStorage::setSoundsToPlay(std::vector<TSoundToPlay> sounds, const int64_t timestampMs)
{
    m_soundsToPlay = std::move(sounds);
    m_timestampMs = timestampMs;
    Q_EMIT soundsToPlayChanged();
}

std::vector<TSoundToPlay> TSoundStorage::takeSoundsToPlay()
{
    std::vector<TSoundToPlay> sounds;
    sounds.reserve(m_soundsToPlay.size());
    for (const TSoundToPlay& sound : m_soundsToPlay)
        sounds.push_back(sound);
    m_soundsToPlay.clear();
    return sounds;
}

void TSoundStorage::queueSound(const TSoundToPlay& sound)
{
    m_queuedSounds.push_back(sound);
}

void TSoundStorage::update(const int64_t nowMs)
{
    const auto due = std::stable_partition(m_queuedSounds.begin(), m_queuedSounds.end(),
                                           [nowMs](const TSoundToPlay& sound) { return sound.timestampMs > nowMs; });
    if (due == m_queuedSounds.end())
        return;
    std::move(due, m_queuedSounds.end(), std::back_inserter(m_soundsToPlay));
    m_queuedSounds.erase(due, m_queuedSounds.end());
    Q_EMIT soundsToPlayChanged();
}

}
