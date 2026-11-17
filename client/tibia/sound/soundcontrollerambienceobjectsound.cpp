#include "sound/soundcontrollerambienceobjectsound.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSoundObjects, "sound.objects", QtWarningMsg)

namespace tibia::sound {

namespace {

/// The distance modifier: `max` minus `step` per field, within 0..100.
uint8_t distanceModifier(const int max, const int step, const int distance)
{
    const int value = max - step * distance;
    if (value < 0)
        return 0;
    return static_cast<uint8_t>(std::min(value, 100));
}

}

TSoundControllerAmbienceObjectSound::TSoundControllerAmbienceObjectSound(
    std::shared_ptr<TSoundMetadataManager> metadata, std::shared_ptr<TSoundManager> soundManager,
    std::weak_ptr<TSoundPlayerManager> playerManager, std::shared_ptr<TSoundConfiguration> configuration)
    : TSoundControllerBase(std::move(metadata), std::move(soundManager), std::move(playerManager), std::move(configuration))
{}

TSoundControllerAmbienceObjectSound::~TSoundControllerAmbienceObjectSound()
{
    for (auto& [id, entry] : m_entries)
        delete entry.player;
}

void TSoundControllerAmbienceObjectSound::update(const int64_t nowMs)
{
    TSoundControllerBase::update(nowMs);
    for (auto& [id, entry] : m_entries) {
        if (entry.player)
            entry.player->update(nowMs);
    }
}

void TSoundControllerAmbienceObjectSound::applyDistance(TActiveSoundPlayer* player, const int nearestDistance)
{
    const TSoundSettings& settings = configuration()->settings();
    player->setDistanceModifier(distanceModifier(settings.distanceVolumeMax, settings.distanceVolumeStep, nearestDistance));
    qCDebug(lcSoundObjects) << "Distance of" << soundManager()->soundName(player->currentSoundId()) << "is" << nearestDistance;
}

void TSoundControllerAmbienceObjectSound::ambienceObjectStreamCountChanged(const uint16_t streamId, const size_t count,
                                                                           const int nearestDistance)
{
    qCDebug(lcSoundObjects) << "TSoundControllerAmbienceObjectSound::ambienceObjectStreamCountChanged for object sound"
                            << streamId << "count" << count << "nearest" << nearestDistance;
    auto it = m_entries.find(streamId);
    if (it == m_entries.end()) {
        const std::optional<TAmbienceObjectStreamMetadata> stream = metadata()->ambienceObjectStream(streamId);
        if (!stream)
            return;
        TEntry entry;
        entry.stream = *stream;
        entry.count = count;
        entry.nearestDistance = nearestDistance;
        it = m_entries.emplace(streamId, std::move(entry)).first;
    } else {
        it->second.nearestDistance = nearestDistance;
        it->second.count = count;
    }

    const TSoundConfiguration::TEffectSettings settings = configuration()->effectSettings(8, std::nullopt, 0);
    if (!settings.enabled || settings.volume == 0.0f)
        return;
    if (!configuration()->isAmbienceEnabled())
        return;

    TEntry& entry = it->second;
    if (!entry.player)
        entry.player = createActivePlayer();
    TActiveSoundPlayer* player = entry.player;

    // The loop of the largest count the objects reach.
    std::optional<uint16_t> soundId;
    if (count != 0 && !entry.stream.loopingSoundIdByCount.empty()) {
        auto upper = entry.stream.loopingSoundIdByCount.upper_bound(static_cast<uint32_t>(count));
        if (upper != entry.stream.loopingSoundIdByCount.begin())
            soundId = std::prev(upper)->second;
    }
    if (!soundId) {
        if (player->isPlaying()) {
            qCDebug(lcSoundObjects) << "TSoundControllerAmbienceObjectSound::ambienceObjectStreamCountChanged: stop"
                                    << soundManager()->soundName(player->currentSoundId()) << "count" << count;
            player->fadeOut();
        }
        return;
    }

    std::optional<TPlayableSound> playable = soundManager()->playableSound(*soundId);
    if (!playable || (!playable->isReady() && !playable->isLoading())) {
        qCDebug(lcSoundObjects) << "TSoundControllerAmbienceObjectSound::ambienceObjectStreamCountChanged: no sound"
                                << soundManager()->soundName(*soundId);
        player->fadeOut();
        return;
    }
    if (player->currentSoundId() != playable->soundId()) {
        const TSoundSettings& soundSettings = configuration()->settings();
        player->setFade(2, soundSettings.ambienceObjectFadeMs, soundSettings.fadeSteps);
        qCDebug(lcSoundObjects) << "TSoundControllerAmbienceObjectSound::ambienceObjectStreamCountChanged: play"
                                << soundManager()->soundName(playable->soundId());
        player->play(*playable);
        player->setLooping(true);
    }
    applyDistance(player, nearestDistance);
    player->setVolume(settings.volume);
}

void TSoundControllerAmbienceObjectSound::stopAllAmbienceObjectSounds()
{
    qCDebug(lcSoundObjects) << "TSoundControllerAmbienceObjectSound::stopAllAmbienceObjectSounds()";
    for (auto& [id, entry] : m_entries) {
        if (!entry.player)
            continue;
        entry.player->stopNow();
        addActivePlayer(entry.player);
        entry.player = nullptr;
    }
}

void TSoundControllerAmbienceObjectSound::setSuspended(const bool suspended)
{
    TSoundControllerBase::setSuspended(suspended);
    if (!suspended)
        return;
    stopAllAmbienceObjectSounds();
    m_entries.clear();
}

void TSoundControllerAmbienceObjectSound::onSoundConfigurationChanged()
{
    const TSoundConfiguration::TEffectSettings settings = configuration()->effectSettings(8, std::nullopt, 0);
    if (settings.enabled && settings.volume != 0.0f && configuration()->isAmbienceEnabled()) {
        for (auto& [id, entry] : m_entries) {
            if (entry.player)
                entry.player->setVolume(settings.volume);
            ambienceObjectStreamCountChanged(id, entry.count, entry.nearestDistance);
        }
        return;
    }
    stopAllAmbienceObjectSounds();
}

}
