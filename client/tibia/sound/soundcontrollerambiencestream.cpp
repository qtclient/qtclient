#include "sound/soundcontrollerambiencestream.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSoundAmbience, "sound.ambience", QtFatalMsg)

namespace tibia::sound {

namespace {

/// A delay of `delayMs` ± half.
int64_t randomDelay(const int64_t delayMs)
{
    return randomBetween(delayMs - delayMs / 2, delayMs / 2 + delayMs);
}

}

TSoundControllerAmbienceStream::TSoundControllerAmbienceStream(std::shared_ptr<TSoundMetadataManager> metadata,
                                                               std::shared_ptr<TSoundManager> soundManager,
                                                               std::weak_ptr<TSoundPlayerManager> playerManager,
                                                               std::shared_ptr<TSoundConfiguration> configuration)
    : TSoundControllerBase(std::move(metadata), std::move(soundManager), std::move(playerManager), std::move(configuration))
{}

TSoundControllerAmbienceStream::~TSoundControllerAmbienceStream()
{
    delete m_player;
}

void TSoundControllerAmbienceStream::update(const int64_t nowMs)
{
    TSoundControllerBase::update(nowMs);
    if (m_player)
        m_player->update(nowMs);
    while (!m_scheduledEffects.empty() && m_scheduledEffects.begin()->first <= nowMs) {
        const TScheduledEffect effect = m_scheduledEffects.begin()->second;
        m_scheduledEffects.erase(m_scheduledEffects.begin());
        TSoundEffectRequest request;
        request.effectId = effect.effectId;
        const TSoundEffectParameters parameters = effectParameters(request);
        if (std::optional<TSoundPlayerToken> token = playSingleShot(parameters)) {
            qDebug() << "Play ambience numeric sound" << QString::number(effect.effectId) << "at" << nowMs;
            keepSingleShot(std::move(*token));
        }
        const int64_t next = randomDelay(effect.delayMs) + now();
        if (effect.effectId != 0) {
            qCDebug(lcSoundAmbience) << "Append future ambience numeric sound" << QString::number(effect.effectId) << "in"
                                     << (next - now()) / 1000 << "seconds";
            m_scheduledEffects.emplace(next, effect);
        }
    }
}

void TSoundControllerAmbienceStream::startStream(const bool updatePlayer)
{
    const TSoundConfiguration::TEffectSettings settings = configuration()->effectSettings(8, std::nullopt, 0);
    if (!settings.enabled || settings.volume == 0.0f)
        return;
    if (!configuration()->isAmbienceStreamEnabled())
        return;
    if (isStreamPlaying(m_requestedStreamId))
        return;
    const std::optional<TAmbienceStreamMetadata> stream = metadata()->ambienceStream(m_requestedStreamId);
    if (!stream)
        return;
    m_stream = *stream;
    if (!m_player)
        m_player = createActivePlayer();

    std::optional<TPlayableSound> playable = soundManager()->playableSound(m_stream.loopingSoundId);
    if (!playable || !playable->isReady()) {
        m_player->fadeOut();
        qCDebug(lcSoundAmbience) << "Stop ambience stream due to invalid looping sound effect id "
                                 << soundManager()->soundName(m_stream.loopingSoundId);
    } else {
        if (updatePlayer)
            m_player->update(now());
        m_player->setLooping(true);
        m_player->setVolume(settings.volume);
        const TSoundSettings& soundSettings = configuration()->settings();
        m_player->setFade(2, soundSettings.ambienceStreamFadeMs, soundSettings.fadeSteps);
        qCDebug(lcSoundAmbience) << "Start ambience stream " << soundManager()->soundName(playable->soundId());
        m_player->play(*playable);
    }

    struct TRoll
    {
        int64_t delayMs;
        TScheduledEffect effect;
    };
    std::vector<TRoll> rolls;
    rolls.reserve(m_stream.delayedEffects.size());
    for (const TDelayedSoundEffectMetadata& delayed : m_stream.delayedEffects)
        rolls.push_back({ randomDelay(delayed.delayMs), { delayed.numericSoundEffectId, delayed.delayMs } });
    if (!rolls.empty()) {
        // The first effect comes after a fresh delay; the others keep their
        // distance to it.
        const auto first = std::min_element(rolls.begin(), rolls.end(),
                                            [](const TRoll& a, const TRoll& b) { return a.delayMs < b.delayMs; });
        const TRoll firstRoll = *first;
        for (TRoll& roll : rolls) {
            if (roll.effect.effectId == firstRoll.effect.effectId && roll.effect.delayMs == firstRoll.effect.delayMs)
                roll.delayMs = randomDelay(roll.effect.delayMs);
            else
                roll.delayMs -= firstRoll.delayMs;
        }
    }
    m_scheduledEffects.clear();
    for (const TRoll& roll : rolls) {
        if (roll.effect.effectId == 0)
            continue;
        m_scheduledEffects.emplace(roll.delayMs + now(), roll.effect);
        qCDebug(lcSoundAmbience) << "Append future ambience sound" << QString::number(roll.effect.effectId) << "in"
                                 << roll.delayMs / 1000 << "seconds";
    }
}

bool TSoundControllerAmbienceStream::isStreamPlaying(const uint16_t streamId) const
{
    return m_stream.id == streamId && m_player && m_player->isPlaying();
}

void TSoundControllerAmbienceStream::setAmbienceStream(const uint16_t streamId)
{
    if (isStreamPlaying(streamId))
        return;
    m_requestedStreamId = streamId;
    if (streamId != 0) {
        startStream(true);
        return;
    }
    if (!m_player)
        return;
    m_player->fadeOut();
    releasePlayer();
    releaseFinishedPlayers(singleShots());
}

void TSoundControllerAmbienceStream::releasePlayer()
{
    addActivePlayer(m_player);
    m_player = nullptr;
    m_scheduledEffects.clear();
}

void TSoundControllerAmbienceStream::stop(const bool immediately)
{
    if (!m_player)
        return;
    if (immediately)
        m_player->stopNow();
    else
        m_player->fadeOut();
    releasePlayer();
    if (immediately) {
        for (const TSoundPlayerToken& token : singleShots()) {
            if (token.player())
                token.player()->stop();
        }
    }
    releaseFinishedPlayers(singleShots());
}

void TSoundControllerAmbienceStream::onSoundConfigurationChanged()
{
    if (!configuration()->isAmbienceEnabled()) {
        stop(true);
        return;
    }
    if (!isStreamPlaying(m_requestedStreamId))
        startStream(false);
    const float volume = configuration()->ambienceVolume();
    if (m_player)
        m_player->setVolume(volume);
    for (const TSoundPlayerToken& token : singleShots()) {
        if (TSoundPlayer* player = token.player()) {
            TVolumeModifiers modifiers = player->volumeModifiers();
            modifiers.volume = volume;
            player->setVolumeModifiers(modifiers);
        }
    }
    for (TActiveSoundPlayer* player : activePlayers())
        player->setVolume(volume);
}

void TSoundControllerAmbienceStream::setSuspended(const bool suspended)
{
    TSoundControllerBase::setSuspended(suspended);
    if (suspended)
        m_requestedStreamId = 0;
}

}
