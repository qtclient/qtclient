#pragma once

// CIP-named screenshot surfaces (promoted from cipwire).

#include "shared/cip_typed.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace screenshot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TOutstandingScreenshot final : public cip::tibia::screenshot::TOutstandingScreenshot
{
    Q_OBJECT
public:
    using cip::tibia::screenshot::TOutstandingScreenshot::TOutstandingScreenshot;

    uint id() const { return m_id; }
    void setId(uint id) { m_id = id; }
    void markReady(const QImage& image);
    QImage image() const { return m_image; }

private:
    uint m_id = 0;
    QImage m_image;
};

/// CIP TScreenshotManager — sync grab or backlog queue when screenshotsUseBacklog.
class TScreenshotManager final : public cip::tibia::screenshot::TScreenshotManager
{
    Q_OBJECT
public:
    explicit TScreenshotManager(QObject* parent = nullptr);
    ~TScreenshotManager() override;

    static TScreenshotManager* instance();

    void configure(QQuickWindow* window, QQuickItem* mapWindowPane);
    /// TAKE_MANUAL / TAKE_AUTOMATIC_SCREENSHOT — returns true when handled.
    bool handleScreenshotAction(const input::TGameAction& action);

public Q_SLOTS:
    void onScreenshotMangerOptionsChanged() override;
    bool takeScreenshotIfEventIsWainting() override;
    void onBacklockUpdateTriggered();
    void processReadyScreenshot(uint screenshotId);

private:
    struct Pending {
        bool automatic = false;
        int eventType = 0;
        uint id = 0;
    };

    bool automaticAllowed(const QJsonObject& opts, int eventType) const;
    void captureNow(const Pending& pending, const QJsonObject& opts);
    void enqueue(const Pending& pending);
    void drainBacklog();
    QString nextPath() const;

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_mapWindowPane;
    QVector<Pending> m_backlog;
    QHash<uint, TOutstandingScreenshot*> m_outstanding;
    QTimer m_backlogTimer;
    uint m_nextId = 1;
    bool m_useBacklog = false;

    static TScreenshotManager* s_instance;
};

}
}
