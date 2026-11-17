#include "sound/activesoundplayer.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcSoundFades, "sound.fades", QtWarningMsg)

namespace tibia::sound {

namespace {

uint8_t fadeLevel(const TSoundPlayer* player)
{
    return player ? player->volumeModifiers().fade : 0;
}

}

TActiveSoundPlayer::TFadingSoundPlayer::TFadingSoundPlayer(TSoundPlayerToken playerToken)
    : token(std::move(playerToken))
{
    if (TSoundPlayer* soundPlayer = player()) {
        TVolumeModifiers modifiers = soundPlayer->volumeModifiers();
        modifiers.fade = 0;
        soundPlayer->setVolumeModifiers(modifiers);
    }
}

void TActiveSoundPlayer::TFadingSoundPlayer::setDirection(const int newDirection, const int64_t nowMs)
{
    direction = newDirection;
    uint8_t level = fadeLevel(player());
    if (direction != 0)
        level = static_cast<uint8_t>(100 - level);
    startMs = nowMs - static_cast<int64_t>((static_cast<double>(level) / 100.0) * static_cast<double>(durationMs));
}

void TActiveSoundPlayer::TFadingSoundPlayer::setFade(const int newDirection, const int64_t newDurationMs,
                                                     const uint8_t newSteps)
{
    direction = newDirection;
    durationMs = newDurationMs;
    steps = newSteps;
}

void TActiveSoundPlayer::TFadingSoundPlayer::updateFade(const int64_t nowMs)
{
    TSoundPlayer* soundPlayer = player();
    if (!soundPlayer)
        return;
    const uint8_t current = soundPlayer->volumeModifiers().fade;
    const int64_t end = durationMs + startMs;
    double value = 1.0;
    if (nowMs <= end && durationMs != 0 && nowMs < end) {
        if (steps == 0) {
            value = 0.0;
        } else {
            const double progress = (static_cast<double>(nowMs - startMs) / static_cast<double>(durationMs)) * steps;
            value = std::round(progress) / steps;
        }
    }
    if (direction == 1)
        value = 1.0 - value;
    const auto next = static_cast<uint8_t>(static_cast<int>(value * 100.0));
    if (current == next)
        return;
    if (soundPlayer->sound() && lcSoundFades().isDebugEnabled())
        qCDebug(lcSoundFades) << "Fading" << soundPlayer->sound()->metadata().fileName << "from" << current << "to" << next;
    TVolumeModifiers modifiers = soundPlayer->volumeModifiers();
    modifiers.fade = next;
    soundPlayer->setVolumeModifiers(modifiers);
}

TActiveSoundPlayer::TActiveSoundPlayer(std::weak_ptr<TSoundPlayerManager> playerManager, QObject* parent)
    : cip::tibia::sound::TActiveSoundPlayer(parent)
    , m_playerManager(std::move(playerManager))
{}

TActiveSoundPlayer::~TActiveSoundPlayer()
{
    if (m_currentPlayer && m_currentPlayer->player())
        m_currentPlayer->player()->reset();
}

void TActiveSoundPlayer::setFade(const int fadeType, const int64_t durationMs, const uint8_t steps)
{
    m_fadeType = fadeType;
    m_fadeDurationMs = durationMs;
    m_fadeSteps = steps;
    if (m_currentPlayer)
        m_currentPlayer->setFade(0, durationMs, steps);
    for (TFadingSoundPlayer& fading : m_fadingPlayers)
        fading.setFade(1, durationMs, steps);
}

void TActiveSoundPlayer::setVolume(float volume)
{
    volume = std::clamp(volume, 0.0f, 100.0f);
    if (m_volume == volume)
        return;
    m_volume = volume;
    updateVolume();
    Q_EMIT volumeChanged();
}

void TActiveSoundPlayer::updateVolume()
{
    const auto apply = [this](TSoundPlayer* player) {
        if (!player)
            return;
        TVolumeModifiers modifiers = player->volumeModifiers();
        modifiers.volume = m_volume;
        player->setVolumeModifiers(modifiers);
    };
    if (m_currentPlayer)
        apply(m_currentPlayer->player());
    for (const TFadingSoundPlayer& fading : m_fadingPlayers)
        apply(fading.player());
    Q_EMIT volumeChanged();
}

void TActiveSoundPlayer::setDistanceModifier(const uint8_t distanceModifier)
{
    if (m_distanceModifier == distanceModifier)
        return;
    m_distanceModifier = distanceModifier;
    updateDistanceModifier();
    Q_EMIT volumeChanged();
}

void TActiveSoundPlayer::updateDistanceModifier()
{
    const auto apply = [this](TSoundPlayer* player) {
        if (!player)
            return;
        TVolumeModifiers modifiers = player->volumeModifiers();
        modifiers.distance = m_distanceModifier;
        player->setVolumeModifiers(modifiers);
    };
    if (m_currentPlayer)
        apply(m_currentPlayer->player());
    for (const TFadingSoundPlayer& fading : m_fadingPlayers)
        apply(fading.player());
    Q_EMIT volumeChanged();
}

void TActiveSoundPlayer::setPitch(const float pitch)
{
    if (m_pitch == pitch)
        return;
    m_pitch = pitch;
    updatePitch();
}

void TActiveSoundPlayer::updatePitch()
{
    if (m_currentPlayer && m_currentPlayer->player())
        m_currentPlayer->player()->setPitch(m_pitch);
    for (const TFadingSoundPlayer& fading : m_fadingPlayers) {
        if (fading.player())
            fading.player()->setPitch(m_pitch);
    }
}

void TActiveSoundPlayer::setLooping(const bool looping)
{
    if (m_looping == looping)
        return;
    m_looping = looping;
    updateLooping();
}

void TActiveSoundPlayer::updateLooping()
{
    if (m_currentPlayer && m_currentPlayer->player())
        m_currentPlayer->player()->setLooping(m_looping);
    for (const TFadingSoundPlayer& fading : m_fadingPlayers) {
        if (fading.player())
            fading.player()->setLooping(m_looping);
    }
}

void TActiveSoundPlayer::play(const TPlayableSound& sound)
{
    m_nextSound = sound;
    startPlaying();
}

void TActiveSoundPlayer::setNextSound(const TPlayableSound& sound)
{
    m_nextSound = sound;
}

std::optional<TActiveSoundPlayer::TFadingSoundPlayer> TActiveSoundPlayer::acquirePlayer()
{
    const std::shared_ptr<TSoundPlayerManager> manager = m_playerManager.lock();
    if (!manager || !m_nextSound || !m_nextSound->isReady())
        return std::nullopt;
    std::optional<TSoundPlayerToken> token = manager->acquirePlayer();
    if (!token)
        return std::nullopt;
    return TFadingSoundPlayer(std::move(*token));
}

void TActiveSoundPlayer::startPlaying()
{
    if (!m_nextSound)
        return;
    for (;;) {
        if (!m_nextSound->isReady())
            return;
        if (!m_currentPlayer)
            break;
        if (m_currentPlayer->player() && m_currentPlayer->player()->hasSound(*m_nextSound)) {
            m_nextSound.reset();
            return;
        }
        fadeOut();
        if (!m_nextSound)
            return;
    }

    // A fading player with the wanted sound fades back in.
    for (auto it = m_fadingPlayers.begin(); it != m_fadingPlayers.end(); ++it) {
        if (!it->player() || !it->player()->hasSound(*m_nextSound))
            continue;
        TFadingSoundPlayer adopted = std::move(*it);
        m_fadingPlayers.erase(it);
        m_currentPlayer.emplace(std::move(adopted));
        m_currentPlayer->setDirection(0, m_nowMs);
        m_nextSound.reset();
        break;
    }

    if (!m_currentPlayer) {
        if (std::optional<TFadingSoundPlayer> acquired = acquirePlayer())
            m_currentPlayer.emplace(std::move(*acquired));
        if (m_currentPlayer) {
            TSoundPlayer* player = m_currentPlayer->player();
            player->setSound(*m_nextSound);
            player->setLooping(m_looping);
            player->setPitch(m_pitch);
            player->play();
            m_currentPlayer->setDirection(0, m_nowMs);
        }
        m_nextSound.reset();
    }

    updateDistanceModifier();
    updateVolume();
    if (m_currentPlayer)
        m_currentPlayer->setFade(0, m_fadeDurationMs, m_fadeSteps);
    for (TFadingSoundPlayer& fading : m_fadingPlayers)
        fading.setFade(1, m_fadeDurationMs, m_fadeSteps);
}

void TActiveSoundPlayer::fadeOut()
{
    if (!m_currentPlayer)
        return;
    m_currentPlayer->setDirection(1, m_nowMs);
    m_fadingPlayers.push_back(std::move(*m_currentPlayer));
    m_currentPlayer.reset();
}

void TActiveSoundPlayer::stopNow()
{
    fadeOut();
    for (TFadingSoundPlayer& fading : m_fadingPlayers) {
        fading.startMs -= fading.durationMs;
        fading.updateFade(m_nowMs);
    }
}

void TActiveSoundPlayer::reset()
{
    const auto stopPlayer = [](TSoundPlayer* player) {
        if (!player)
            return;
        player->stop();
        player->reset();
    };
    if (m_currentPlayer)
        stopPlayer(m_currentPlayer->player());
    for (const TFadingSoundPlayer& fading : m_fadingPlayers)
        stopPlayer(fading.player());
    m_nextSound.reset();
    m_finishedNotified = false;
    m_distanceModifier = 100;
    m_volume = 100.0f;
    updateDistanceModifier();
    updateVolume();
}

void TActiveSoundPlayer::update(const int64_t nowMs)
{
    m_nowMs = nowMs;
    if (m_currentPlayer)
        m_currentPlayer->updateFade(nowMs);
    else if (m_nextSound)
        startPlaying();

    for (auto it = m_fadingPlayers.begin(); it != m_fadingPlayers.end();) {
        if (it->isFinished(nowMs)) {
            it = m_fadingPlayers.erase(it);
            continue;
        }
        it->updateFade(nowMs);
        ++it;
        Q_EMIT playingChanged();
        Q_EMIT finished();
    }
    if (!m_fadingPlayers.empty() || (m_currentPlayer && m_currentPlayer->player() && m_currentPlayer->player()->isPlaying()))
        Q_EMIT playingOffsetChanged();
}

bool TActiveSoundPlayer::isPlaying() const
{
    if (!m_fadingPlayers.empty())
        return true;
    if (m_currentPlayer && m_currentPlayer->player())
        return m_currentPlayer->player()->isPlaying();
    return false;
}

TSoundEffectID TActiveSoundPlayer::currentSoundId() const
{
    if (m_currentPlayer) {
        const TSoundPlayer* player = m_currentPlayer->player();
        if (player && player->sound())
            return player->sound()->soundId();
        return 0;
    }
    if (m_nextSound && m_nextSound->isReady())
        return m_nextSound->soundId();
    return 0;
}

uint8_t TActiveSoundPlayer::currentFadeLevel() const
{
    if (m_currentPlayer)
        return fadeLevel(m_currentPlayer->player());
    return lastFadingLevel();
}

uint8_t TActiveSoundPlayer::lastFadingLevel() const
{
    return m_fadingPlayers.empty() ? 0 : fadeLevel(m_fadingPlayers.back().player());
}

TSoundEffectID TActiveSoundPlayer::lastFadingSoundId() const
{
    if (m_fadingPlayers.empty())
        return 0;
    const TSoundPlayer* player = m_fadingPlayers.back().player();
    return player && player->sound() ? player->sound()->soundId() : 0;
}

int64_t TActiveSoundPlayer::playingOffsetMs() const
{
    if (m_currentPlayer && m_currentPlayer->player() && m_currentPlayer->player()->isPlaying())
        return m_currentPlayer->player()->playingOffsetMs();
    return -1;
}

int64_t TActiveSoundPlayer::durationMs() const
{
    if (m_currentPlayer && m_currentPlayer->player())
        return m_currentPlayer->player()->durationMs();
    return 0;
}

void TActiveSoundPlayer::setPlayingOffsetMs(const int64_t offsetMs)
{
    if (m_currentPlayer && m_currentPlayer->player() && m_currentPlayer->player()->isPlaying())
        m_currentPlayer->player()->setPlayingOffsetMs(offsetMs);
}

}
