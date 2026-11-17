#include "sound/soundcontrollermusic.h"

#include <QLoggingCategory>

#include <limits>

Q_LOGGING_CATEGORY(lcSoundMusic, "sound.music", QtWarningMsg)

namespace tibia::sound {

namespace {

bool isImmediateOrTitle(const std::optional<TMusicTemplateMetadata>& music)
{
    return music
        && (music->musicType == TSoundControllerMusic::ImmediateMusic || music->musicType == TSoundControllerMusic::TitleMusic);
}

int64_t randomSeconds(const TSecondsRange& range)
{
    return randomBetween(static_cast<int64_t>(range.min), static_cast<int64_t>(range.max));
}

}

TSoundControllerMusic::TSoundControllerMusic(std::shared_ptr<TSoundMetadataManager> metadata,
                                             std::shared_ptr<TSoundManager> soundManager,
                                             std::weak_ptr<TSoundPlayerManager> playerManager,
                                             std::shared_ptr<TSoundConfiguration> configuration)
    : TSoundControllerBase(std::move(metadata), std::move(soundManager), playerManager, std::move(configuration))
    , m_activePlayer(playerManager)
{
    const TSoundSettings& settings = this->configuration()->settings();
    m_activePlayer.setFade(1, settings.musicFadeMs, settings.fadeSteps);
}

void TSoundControllerMusic::update(const int64_t nowMs)
{
    TSoundControllerBase::update(nowMs);
    m_activePlayer.update(nowMs);
    m_nowSeconds = nowMs / 1000;
    updateMusic();
}

void TSoundControllerMusic::updateMusic()
{
    const bool wasPlaying = m_wasPlaying;
    m_wasPlaying = m_activePlayer.isPlaying();
    if (wasPlaying && !m_wasPlaying)
        onMusicFinished();

    const TSoundSettings& settings = configuration()->settings();
    const int64_t maxSeconds =
        settings.maxMusicDurationSeconds > 0 ? settings.maxMusicDurationSeconds : std::numeric_limits<int64_t>::max();
    if (maxSeconds != std::numeric_limits<int64_t>::max() && m_activePlayer.isPlaying()) {
        // Plays the first and the last half of the longest play time.
        const int64_t half = maxSeconds * 500;
        const int64_t duration = m_activePlayer.durationMs();
        const int64_t offset = m_activePlayer.playingOffsetMs();
        if (half < duration && half < offset && offset < duration - half) {
            qCDebug(lcSoundMusic) << "Skipping to the end of" << soundManager()->soundName(m_currentTemplate ? m_currentTemplate->soundId : 0);
            m_activePlayer.setPlayingOffsetMs(duration - half);
        }
    }

    if (m_nextMusicSeconds <= m_nowSeconds) {
        if (!m_pendingTemplate) {
            if (!m_wasPlaying && m_requestedTemplate) {
                bool wait = false;
                if (m_requestedTemplate->musicType == TitleMusic) {
                    wait = !configuration()->isTitleMusicEnabled();
                } else if (isSuspended()) {
                    wait = true;
                } else if (m_playerFlag && settings.musicVolumeWhileFlagged == 0 && m_requestedTemplate->musicType == Music) {
                    wait = true;
                } else if (const auto it = m_templateCooldownSeconds.find(m_requestedTemplate->id);
                           it != m_templateCooldownSeconds.end() && m_nowSeconds < it->second) {
                    wait = true;
                }
                if (!wait)
                    m_pendingTemplate = m_requestedTemplate;
            }
        } else {
            m_currentTemplate = m_pendingTemplate;
            m_pendingTemplate.reset();
            startMusic(m_currentTemplate->soundId);
        }
    }

    if (m_activePlayer.isPlaying()) {
        uint8_t distanceModifier = 100;
        if (m_currentTemplate && m_currentTemplate->musicType == Music)
            distanceModifier = settings.musicVolumeWhileFlagged;
        m_activePlayer.setDistanceModifier(distanceModifier);
    }
}

void TSoundControllerMusic::onMusicFinished()
{
    qCDebug(lcSoundMusic) << "Music finished"
                          << soundManager()->soundName(m_currentTemplate ? m_currentTemplate->soundId : 0);
    m_activePlayer.reset();
    if (m_currentTemplate) {
        const TSoundSettings& settings = configuration()->settings();
        int64_t cooldown = 0;
        if (m_currentTemplate->musicType == ImmediateMusic)
            cooldown = randomSeconds(settings.immediateMusicCooldown);
        else if (m_currentTemplate->musicType == Music)
            cooldown = randomSeconds(settings.musicCooldown);
        if (!isSuspended()) {
            qCDebug(lcSoundMusic) << "Music template" << m_currentTemplate->id << "cooldown" << cooldown << "seconds";
            m_templateCooldownSeconds[m_currentTemplate->id] = cooldown + m_nowSeconds;
        }
    }
    int pause = LongPause;
    if (isImmediateOrTitle(m_requestedTemplate))
        pause = Immediate;
    else if (m_currentTemplate && m_currentTemplate->musicType == TitleMusic && m_requestedTemplate)
        pause = m_requestedTemplate->musicType != Music ? LongPause : ShortPause;
    setPause(pause);
    m_currentTemplate.reset();
}

void TSoundControllerMusic::setPause(const int pause)
{
    const TSoundSettings& settings = configuration()->settings();
    int64_t seconds = 0;
    switch (pause) {
    case Immediate:
        qCDebug(lcSoundMusic) << "setting music pause to immediate (0 seconds)";
        break;
    case ShortPause:
        seconds = randomSeconds(settings.shortMusicPause);
        qCDebug(lcSoundMusic) << "setting music pause to" << seconds << "seconds";
        break;
    case LongPause:
        seconds = randomSeconds(settings.longMusicPause);
        qCDebug(lcSoundMusic) << "setting music pause to" << seconds << "seconds";
        break;
    default:
        qCDebug(lcSoundMusic) << "setting music pause to unknown";
        break;
    }
    m_nextMusicSeconds = seconds + m_nowSeconds;
}

void TSoundControllerMusic::startMusic(const TSoundEffectID soundId)
{
    if (!configuration()->isMusicEnabled())
        return;
    const float volume = configuration()->musicVolume();
    if (volume == 0.0f) {
        m_activePlayer.fadeOut();
        return;
    }
    std::optional<TPlayableSound> playable = soundManager()->playableSound(soundId);
    if (!playable || !playable->isReady())
        return;
    if (playable->metadata().fileName.isEmpty()) {
        qCDebug(lcSoundMusic) << "Music" << soundManager()->soundName(soundId) << "has no file";
    } else if (!m_activePlayer.isPlaying()) {
        m_activePlayer.setVolume(volume);
        const char* kind = "music";
        if (m_currentTemplate && m_currentTemplate->musicType == ImmediateMusic)
            kind = "immediate music";
        else if (m_currentTemplate && m_currentTemplate->musicType == TitleMusic)
            kind = "title music";
        qCDebug(lcSoundMusic) << "Start" << kind << soundManager()->soundName(soundId);
        m_activePlayer.play(*playable);
        return;
    } else {
        if (m_activePlayer.currentSoundId() == soundId)
            return;
        m_activePlayer.setVolume(volume);
        m_activePlayer.setNextSound(*playable);
        qCDebug(lcSoundMusic) << "Cross-fade to" << soundManager()->soundName(soundId);
    }
    m_activePlayer.fadeOut();
}

void TSoundControllerMusic::stopMusic(const bool immediately)
{
    if (!m_activePlayer.isPlaying())
        return;
    qCDebug(lcSoundMusic) << "Stop music" << soundManager()->soundName(m_currentTemplate ? m_currentTemplate->soundId : 0);
    if (immediately)
        m_activePlayer.stopNow();
    else
        m_activePlayer.fadeOut();
}

void TSoundControllerMusic::setCurrentMusicTemplate(const uint16_t templateId)
{
    m_requestedTemplate = metadata()->musicTemplate(templateId);
    qCDebug(lcSoundMusic) << "TSoundControllerMusic::setCurrentMusicTemplate: New Music Template" << templateId;
    const bool otherTemplate = m_currentTemplate && m_currentTemplate->id != templateId;
    if (otherTemplate && m_currentTemplate->musicType == ImmediateMusic && m_nextMusicSeconds > m_nowSeconds)
        setPause(LongPause);
    else if (otherTemplate && m_currentTemplate->musicType == TitleMusic)
        setPause(ShortPause);
    else if (isImmediateOrTitle(m_requestedTemplate))
        setPause(Immediate);
    if (otherTemplate && (isImmediateOrTitle(m_requestedTemplate) || isImmediateOrTitle(m_currentTemplate)))
        stopMusic(false);
}

void TSoundControllerMusic::setSuspended(const bool suspended)
{
    TSoundControllerBase::setSuspended(suspended);
    if (!suspended) {
        if (m_requestedTemplate && m_requestedTemplate->musicType == TitleMusic)
            m_requestedTemplate.reset();
        setPause(ShortPause);
        return;
    }
    m_templateCooldownSeconds.clear();
    qCDebug(lcSoundMusic) << "Music cooldowns cleared";
    if (const std::optional<TMusicTemplateMetadata> title = metadata()->titleMusicTemplate()) {
        setCurrentMusicTemplate(title->id);
        setPause(Immediate);
    }
}

void TSoundControllerMusic::onSoundConfigurationChanged()
{
    const bool enabled = configuration()->isMusicEnabled();
    const float volume = configuration()->musicVolume();
    const bool titleEnabled = configuration()->isTitleMusicEnabled();
    if (!enabled)
        stopMusic(true);
    else
        m_activePlayer.setVolume(volume);
    if (!titleEnabled) {
        if (m_currentTemplate && m_currentTemplate->musicType == TitleMusic)
            stopMusic(true);
    } else if (isSuspended()) {
        if (const std::optional<TMusicTemplateMetadata> title = metadata()->titleMusicTemplate()) {
            setCurrentMusicTemplate(title->id);
            setPause(Immediate);
        }
    }
    updateMusic();
}

void TSoundControllerMusic::setPlayerFlag(const bool flag)
{
    m_playerFlag = flag;
    if (flag && m_currentTemplate && m_currentTemplate->musicType == Music
        && configuration()->settings().musicVolumeWhileFlagged == 0)
        stopMusic(false);
    updateMusic();
}

}
