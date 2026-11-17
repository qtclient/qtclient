#include "sessiondump/sessiondumpplayer.h"

#include "sessiondump/sessiondumpprogressclockservice.h"

#include "clockservice.h"

#include <protocol.pb.h>

#include <QScopeGuard>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>

namespace tibia::sessiondump {

namespace {

constexpr int64_t NoTimestamp = std::numeric_limits<int64_t>::max();
constexpr int64_t ProcessingBudgetMillis = 200;
/// Above this speed effect and text messages are dropped.
constexpr double FastPlayspeed = 16.0;
/// Message mode the replay never shows.
constexpr uint32_t HiddenMessageMode = 42;
constexpr int GraphicalEffectsMessage = 131;
constexpr int MessageMessage = 180;

}

TSessiondumpPlayer::TSessiondumpPlayer(std::shared_ptr<TSessiondumpAsyncLoader> loader,
                                       std::shared_ptr<TSessiondumpProgressClockService> progressClock)
    : m_loader(std::move(loader))
    , m_progressClock(std::move(progressClock))
    , m_pauseAt(NoTimestamp)
{
}

TSessiondumpPlayer::~TSessiondumpPlayer()
{
    for (const auto& connection : m_connections)
        disconnect(connection);
}

void TSessiondumpPlayer::processSessiondump()
{
    if (m_processing)
        return;
    m_processing = true;
    const auto guard = qScopeGuard([this] { m_processing = false; });

    const int64_t previousPosition = m_position;
    const int64_t startMillis = m_clockService->nowMillis();
    int64_t position = progress();
    m_position = position;
    if (m_pauseAt != NoTimestamp && m_pauseAt <= position) {
        m_progressClock->pauseProgress();
        m_progressClock->setProgress(m_pauseAt);
        m_position = m_pauseAt;
        Q_EMIT sessiondumpProcessingChanged();
        position = m_position;
    }

    int64_t keyframe = 0;
    int64_t target = position;
    bool seekKeyframe = false;
    const auto current = m_keyframes.upper_bound(position);
    if (current != m_keyframes.begin())
        keyframe = std::prev(current)->first;
    if (m_skipTo && m_skipTo->active) {
        const int64_t skipTimestamp = m_skipTo->timestamp;
        const auto next = m_keyframes.upper_bound(skipTimestamp);
        bool reset = false;
        uint32_t seekPosition = 0;
        if (next == m_keyframes.begin()) {
            if (keyframe != 0 || skipTimestamp < previousPosition) {
                keyframe = 0;
                seekPosition = m_loader->packetsBegin();
                reset = true;
            }
        } else {
            const auto skipKeyframe = std::prev(next);
            if (skipKeyframe->first != keyframe || skipTimestamp < previousPosition) {
                keyframe = skipKeyframe->first;
                seekPosition = skipKeyframe->second;
                seekKeyframe = true;
                reset = true;
            }
        }
        if (reset) {
            m_loader->seek(seekPosition);
            m_progressClock->setProgress(keyframe);
            position = progress();
            target = position;
            Q_EMIT requestResetOfStorages();
        }
        if (position < skipTimestamp)
            target = skipTimestamp;
    }

    const int64_t before = m_loader->currentTimestamp();
    m_lagging = !m_loader->processUntil(target, ProcessingBudgetMillis, seekKeyframe);
    int64_t after = m_loader->currentTimestamp();
    if (m_skipTo && m_skipTo->timestamp <= m_loader->nextTimestamp()) {
        m_position = m_skipTo->timestamp;
        onSkipToMarkerReached();
    }
    if (m_lagging) {
        m_position = m_loader->nextTimestamp();
        m_progressClock->setProgress(m_loader->nextTimestamp());
        const int64_t elapsed = m_clockService->nowMillis() - startMillis;
        m_measuredPlayspeed = double(after - before) / double(elapsed);
    }
    if (after != before) {
        m_progressClock->setProgress(std::max(m_position, after));
        Q_EMIT sessiondumpPacketsProcessed();
    }
}

void TSessiondumpPlayer::setPauseAt(const int64_t timestamp) { m_pauseAt = timestamp; }

int64_t TSessiondumpPlayer::pauseAt() const { return m_pauseAt; }

int64_t TSessiondumpPlayer::progress() const { return m_progressClock->progress(); }

int64_t TSessiondumpPlayer::position() const { return m_position; }

void TSessiondumpPlayer::start()
{
    TSessiondumpAsyncLoader* loader = m_loader.get();
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::sessiondumpMetadataLoaded, this,
                                    &TSessiondumpPlayer::onSessiondumpMetadataLoaded));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::finished, this, &TSessiondumpPlayer::onSessiondumpLoaded));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::gameserverMessageAvailable, this,
                                    &TSessiondumpPlayer::onGameserverMessageAvailable));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::gameclientMessageAvailable, this,
                                    &TSessiondumpPlayer::gameclientMessageAvailable));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::preprocessingMessageAvailable, this,
                                    &TSessiondumpPlayer::preprocessingMessageAvailable));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::sessiondumpFinished, this,
                                    &TSessiondumpPlayer::onSessiondumpFinished));
    m_connections.push_back(connect(loader, &TSessiondumpAsyncLoader::sessiondumpLoadingError, this,
                                    &TSessiondumpPlayer::sessiondumpLoadingError));
    m_clockService = std::make_unique<shared::TClockService>();
    m_progressClock->pauseProgress();
    m_loader->startLoading();
    Q_EMIT sessiondumpProcessingChanged();
}

void TSessiondumpPlayer::pause()
{
    if (m_progressClock->isProgressRunning()) {
        m_progressClock->pauseProgress();
        Q_EMIT sessiondumpProcessingChanged();
    }
}

void TSessiondumpPlayer::resume()
{
    m_pauseAt = NoTimestamp;
    if (m_progressClock->isProgressRunning())
        return;
    m_progressClock->resumeProgress();
    Q_EMIT sessiondumpProcessingChanged();
}

bool TSessiondumpPlayer::isRunning() const { return m_progressClock->isProgressRunning(); }

void TSessiondumpPlayer::pauseClock()
{
    m_progressClock->pause();
    Q_EMIT paused();
}

void TSessiondumpPlayer::resumeClock()
{
    if (m_progressClock->speed() == 0.0)
        m_progressClock->resume();
}

bool TSessiondumpPlayer::isPaused() const { return !m_progressClock->isProgressRunning(); }

void TSessiondumpPlayer::faster()
{
    resume();
    setPlayspeed(std::max(m_progressClock->speed() * 2.0, 1.0));
}

void TSessiondumpPlayer::fastest()
{
    resume();
    const double speed = (double(uint64_t(m_progressClock->speed()) >> 3) + 1.0) * 8.0;
    setPlayspeed(std::max(speed, 1.0));
}

void TSessiondumpPlayer::setPlayspeed(const double playspeed)
{
    if (m_progressClock->speed() == playspeed)
        return;
    m_progressClock->setSpeed(playspeed);
    Q_EMIT currentPlayspeedChanged();
}

double TSessiondumpPlayer::currentPlayspeed() const
{
    if (m_lagging)
        return m_measuredPlayspeed;
    return m_progressClock->speed();
}

void TSessiondumpPlayer::skipTo(const double playspeed, const int64_t timestamp)
{
    m_skipTo = std::make_unique<TSkipTo>(TSkipTo{ timestamp, false, true, playspeed });
    Q_EMIT skipToMarkerChanged();
}

void TSessiondumpPlayer::cancelSkipTo()
{
    m_skipTo.reset();
    Q_EMIT skipToMarkerChanged();
}

bool TSessiondumpPlayer::isFinishedLoading() const { return m_finishedLoading; }

int64_t TSessiondumpPlayer::duration() const { return m_duration; }

void TSessiondumpPlayer::addKeyframe(const int64_t& timestamp, const uint32_t position) { m_keyframes[timestamp] = position; }

void TSessiondumpPlayer::setDuration(const int64_t& duration)
{
    if (m_duration < duration) {
        m_duration = duration;
        Q_EMIT sessiondumpInfoChanged();
    }
}

void TSessiondumpPlayer::setLoadedDuration(const int64_t& duration)
{
    if (m_loadedDuration == duration)
        return;
    setDuration(duration);
    if (m_loadedDuration < duration) {
        m_loadedDuration = duration;
        Q_EMIT sessiondumpInfoChanged();
    }
}

double TSessiondumpPlayer::loadingProgress() const
{
    if (m_duration != 0)
        return double(m_loadedDuration) / double(m_duration);
    return 0.0;
}

bool TSessiondumpPlayer::isSkipping() const { return m_skipTo != nullptr; }

int64_t TSessiondumpPlayer::skipToTimestamp() const { return m_skipTo ? m_skipTo->timestamp : NoTimestamp; }

const TSessiondumpMetadata& TSessiondumpPlayer::metadata() const
{
    static const TSessiondumpMetadata empty;
    if (m_loader)
        return m_loader->metadata();
    return empty;
}

void TSessiondumpPlayer::handleGameAction(const input::TGameActionPtr pGameAction)
{
    if (pGameAction->type == input::EGameAction::SESSIONDUMP_PAUSE) {
        pause();
        return;
    }
    if (pGameAction->type == input::EGameAction::SESSIONDUMP_RESUME) {
        resume();
        return;
    }
    if (pGameAction->type == input::EGameAction::SESSIONDUMP_SKIP_TO) {
        const auto skip = std::dynamic_pointer_cast<const input::TGameActionSessiondumpSkipTo>(pGameAction);
        if (!skip)
            return;
        // TGameActionSessiondumpSkipTo carries the timestamp in `parameter`.
        skipTo(std::min(m_progressClock->speed(), 1.0), int64_t(skip->parameter));
    }
}

void TSessiondumpPlayer::onSessiondumpMetadataLoaded()
{
    const TSessiondumpMetadata metadata = m_loader->metadata();
    m_progressClock->pause();
    m_progressClock->setStartTime(metadata.creationTime);
    m_progressClock->setElapsedNanos(0);
    m_progressClock->start();
    m_loader->startProcessing();
    m_progressClock->resume();
    m_started = true;
}

void TSessiondumpPlayer::onSessiondumpLoaded() { Q_EMIT finishedLoadingSessiondump(); }

void TSessiondumpPlayer::onGameserverMessageAvailable(TGameserverMessagePointer pGameserverMessage)
{
    static const std::set<int> SkippableTypes{ GraphicalEffectsMessage, MessageMessage };
    const int type = pGameserverMessage->type();
    if (SkippableTypes.count(type)) {
        if (m_skipTo && m_skipTo->active)
            return;
        if (FastPlayspeed < m_progressClock->speed())
            return;
    }
    if (type == MessageMessage) {
        const auto& message = pGameserverMessage->GetExtension(protobuf::protocol::GameserverMessageMessage::gameserver_message_message);
        if (message.mode() == HiddenMessageMode)
            return;
    }
    Q_EMIT gameserverMessageAvailable(std::move(pGameserverMessage));
}

void TSessiondumpPlayer::onSkipToMarkerReached()
{
    if (!m_skipTo)
        return;
    if (m_skipTo->pauseWhenReached) {
        m_progressClock->pauseProgress();
        Q_EMIT sessiondumpProcessingChanged();
    }
    setPlayspeed(m_skipTo->playspeed);
    m_skipTo.reset();
    Q_EMIT skipToMarkerChanged();
}

void TSessiondumpPlayer::onSessiondumpFinished()
{
    pause();
    setPlayspeed(0.0);
    Q_EMIT finished();
}

}
