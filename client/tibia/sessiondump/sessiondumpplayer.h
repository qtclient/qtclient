#pragma once

#include "sessiondump/sessiondumpasyncloader.h"

#include "input/gameaction.h"

#include <QObject>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace tibia {
namespace shared {
class IClockService;
}
namespace sessiondump {

class TSessiondumpProgressClockService;

/// CIP `sessiondump::TSessiondumpPlayer`: plays a dump the loader reads on a
/// progress clock. Each processSessiondump() call processes the packets up to
/// the clock's progress (at most 200 ms of work), jumping to the keyframe at or
/// before a skip-to target; game actions pause, resume and skip.
class TSessiondumpPlayer : public QObject
{
    Q_OBJECT
public:
    TSessiondumpPlayer(std::shared_ptr<TSessiondumpAsyncLoader> loader,
                       std::shared_ptr<TSessiondumpProgressClockService> progressClock);
    ~TSessiondumpPlayer() override;

    /// Processes the dump up to the progress clock.
    virtual void processSessiondump();
    /// The progress at which playback pauses (INT64_MAX: none).
    virtual void setPauseAt(int64_t timestamp);
    virtual int64_t pauseAt() const;
    virtual int64_t progress() const;
    /// The progress the dump was played to.
    virtual int64_t position() const;
    /// Connects the loader and starts loading, paused.
    virtual void start();
    virtual void pause();
    /// Resumes playback and drops the pause-at timestamp.
    virtual void resume();
    virtual bool isRunning() const;
    /// Stops the clock (the session's own pause).
    virtual void pauseClock();
    virtual void resumeClock();
    virtual bool isPaused() const;
    /// Resumes at twice the speed (at least 1).
    virtual void faster();
    /// Resumes at the next multiple of eight.
    virtual void fastest();
    virtual void setPlayspeed(double playspeed);
    /// The measured speed while processing lags behind the clock, else the clock's speed.
    virtual double currentPlayspeed() const;
    /// Plays fast to `timestamp`, then continues at `playspeed`.
    virtual void skipTo(double playspeed, int64_t timestamp);
    virtual void cancelSkipTo();
    virtual bool isFinishedLoading() const;
    /// The dump's duration so far.
    virtual int64_t duration() const;
    /// A keyframe: the timestamp the packet at file `position` starts.
    virtual void addKeyframe(const int64_t& timestamp, uint32_t position);
    virtual void setDuration(const int64_t& duration);
    virtual void setLoadedDuration(const int64_t& duration);
    /// How much of the duration was loaded (0 to 1).
    virtual double loadingProgress() const;
    virtual bool isSkipping() const;
    /// The loader's metadata, or empty metadata without a loader.
    virtual const TSessiondumpMetadata& metadata() const;

    /// The skip-to target, INT64_MAX without one.
    int64_t skipToTimestamp() const;
    /// The dump's metadata arrived and its processing started (the progress
    /// clock runs on the dump's time from then on).
    bool isStarted() const { return m_started; }

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction) const;
    void preprocessingMessageAvailable(tibia::sessiondump::TSessiondumpPacketInfo PacketInfo,
                                       tibia::sessiondump::TSessiondumpMessageInfo MessageInfo,
                                       std::function<QByteArray()> RawMessageBytesReaderFunction);
    void gameserverMessageAvailable(tibia::sessiondump::TGameserverMessagePointer pGameserverMessage);
    void gameclientMessageAvailable(tibia::sessiondump::TGameclientMessagePointer pGameclientMessage);
    void paused() const;
    void finished() const;
    void sessiondumpInfoChanged();
    void sessiondumpPacketsProcessed();
    void skipToMarkerChanged();
    void requestResetOfStorages();
    void sessiondumpProcessingChanged();
    void sessiondumpLoadingError(QString ErrorMessage);
    void finishedLoadingSessiondump() const;
    void currentPlayspeedChanged() const;

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
    void onSessiondumpMetadataLoaded();
    void onSessiondumpLoaded();
    void onGameserverMessageAvailable(tibia::sessiondump::TGameserverMessagePointer pGameserverMessage);
    void onSkipToMarkerReached();
    void onSessiondumpFinished();

private:
    struct TSkipTo
    {
        int64_t timestamp = 0;
        bool pauseWhenReached = false;
        bool active = true;
        double playspeed = 1.0;
    };

    std::shared_ptr<TSessiondumpAsyncLoader> m_loader;
    std::shared_ptr<TSessiondumpProgressClockService> m_progressClock;
    std::unique_ptr<shared::IClockService> m_clockService;
    bool m_finishedLoading = false;
    int64_t m_pauseAt;
    std::map<int64_t, uint32_t> m_keyframes;
    int64_t m_duration = 0;
    int64_t m_loadedDuration = 0;
    int64_t m_position = 0;
    bool m_lagging = false;
    double m_measuredPlayspeed = 0.0;
    std::unique_ptr<TSkipTo> m_skipTo;
    std::vector<QMetaObject::Connection> m_connections;
    /// CipSoft's named reentrancy guard "TSessiondumpPlayer::processSessiondump()".
    bool m_processing = false;
    bool m_started = false;
};

}
}
