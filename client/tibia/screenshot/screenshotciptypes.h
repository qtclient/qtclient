#pragma once

// CIP-named screenshot surfaces (promoted from cipwire).

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QImage>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>

#include <deque>
#include <forward_list>
#include <memory>
#include <vector>

class QQuickItem;
class QQuickItemGrabResult;
class QQuickWindow;

namespace tibia {
namespace shared {
class TTibiaFileSystemHelper;
}
namespace screenshot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP `TOutstandingScreenshot`: a pending item grab; emits ready(id) once the
/// grab result is ready. Ids come from a process-wide counter.
class TOutstandingScreenshot final : public cip::tibia::screenshot::TOutstandingScreenshot
{
    Q_OBJECT
public:
    TOutstandingScreenshot(const QSharedPointer<QQuickItemGrabResult>& grabResult, int eventType);

    uint id() const { return m_id; }
    const QSharedPointer<QQuickItemGrabResult>& grabResult() const { return m_grabResult; }
    int eventType() const { return m_eventType; }

private:
    uint m_id = 0;
    QSharedPointer<QQuickItemGrabResult> m_grabResult;
    int m_eventType = 0;
};

/// CIP `TScreenshotManager`: grabs the game window when a screenshot event is
/// pending, keeps a backlog of periodic grabs and saves the images on the
/// global thread pool.
class TScreenshotManager final : public cip::tibia::screenshot::TScreenshotManager
{
    Q_OBJECT
public:
    /// CIP event ids (TAKE_AUTOMATIC_SCREENSHOT parameter).
    enum EEvent {
        EVENT_NONE = 0,
        EVENT_ACHIEVEMENT = 1,
        EVENT_BESTIARY_ENTRY_COMPLETED = 2,
        EVENT_BESTIARY_ENTRY_UNLOCKED = 3,
        EVENT_BOSS_DEFEATED = 4,
        EVENT_DEATH_PVE = 5,
        EVENT_DEATH_PVP = 6,
        EVENT_LEVEL_UP = 7,
        EVENT_PLAYER_KILL_ASSIST = 8,
        EVENT_PLAYER_KILL = 9,
        EVENT_PLAYER_ATTACKING = 10,
        EVENT_TREASURE_FOUND = 11,
        EVENT_SKILL_UP = 12,
        EVENT_GIFT_OF_LIFE = 13,
        EVENT_HIGHEST_DAMAGE_DEALT = 50,
        EVENT_HIGHEST_HEALING_DONE = 51,
        EVENT_VALUABLE_LOOT = 52,
        EVENT_LOW_HEALTH = 53,
        EVENT_BACKLOG = 100,
        EVENT_HOTKEY = 101,
    };

    explicit TScreenshotManager(QObject* parent = nullptr);
    ~TScreenshotManager() override;

    static TScreenshotManager* instance();

    /// qtclient wiring: `rootItem` is grabbed for full screenshots, `mapWindow`
    /// when only the game window is wanted.
    void configure(QQuickWindow* window, QQuickItem* mapWindow);
    void setGameSessionActive(bool active);

Q_SIGNALS:
    void publishGameAction(const tibia::input::TGameActionPtr& action);

public Q_SLOTS:
    /// TAKE_MANUAL_SCREENSHOT / TAKE_AUTOMATIC_SCREENSHOT.
    bool handleGameAction(const tibia::input::TGameAction& action);
    void onScreenshotMangerOptionsChanged() override;
    bool takeScreenshotIfEventIsWainting() override;
    void processReadyScreenshot(uint screenshotId);
    void onBacklockUpdateTriggered();

private:
    static bool isScreenshotEnabled(int event);
    void requestScreenshot(int event);
    void clearBacklog();
    void startBacklogTimerIfNeeded();
    void grabScreenshot(int event);
    void saveScreenshots(const std::vector<QImage>& images, const QString& eventName);
    bool isDeathScreenshotDeferred() const;

    shared::TTibiaFileSystemHelper* m_fileSystemHelper = nullptr;
    QPointer<QQuickItem> m_rootItem;
    QPointer<QQuickItem> m_mapWindow;
    QSharedPointer<QQuickItemGrabResult> m_grabResult;
    std::forward_list<std::unique_ptr<TOutstandingScreenshot>> m_outstandingScreenshots;
    int m_pendingEvent = EVENT_NONE;
    int m_heldEvent = EVENT_NONE;
    QTimer m_backlogTimer;
    /// CIP circular buffer of 5 images: the oldest is dropped when full.
    std::deque<QImage> m_backlog;
    bool m_gameSessionActive = false;

    static TScreenshotManager* s_instance;
};

}
}
