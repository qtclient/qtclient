#pragma once

#include "qmlcomponents/outlinetextquickitem.h"

#include "clockservice.h"

#include <QImage>
#include <QQuickItemGrabResult>
#include <QSharedPointer>
#include <QTimer>

#include <cip_gen.h>

#include <array>
#include <deque>
#include <memory>
#include <unordered_map>

class QQuickWindow;

namespace tibia {
namespace qmlcomponents {

/// Renders outline texts into images offscreen (one service for the process).
/// Five invisible outline texts in the window grab the requested text
/// properties one request each; further requests queue. Finished images stay
/// cached by their text properties; above 4 MiB the cleanup drops up to 1 MiB of
/// images that nobody holds any more.
class TOffscreenOutlineTextToImageService : public cip::tibia::qmlcomponents::TOffscreenOutlineTextToImageService
{
    Q_OBJECT
public:
    using TTextProperties = TOutlineTextQuickItem::TTextProperties;

    /// A request: the text properties and, once grabbed, their image.
    struct TTextPropertiesImageToken
    {
        TTextProperties properties;
        QImage image;
        qint64 lastRequested = 0;
        /// Kept in the queue while nobody else holds it.
        bool releaseLater = false;
    };

    static TOffscreenOutlineTextToImageService& instance();

    bool isInitialized() const { return m_initialized; }
    /// Creates the grabbing texts in `window`'s content item.
    void initialize(QQuickWindow* window);

    /// The request for `properties`: one a grabber is working on, a finished one,
    /// a queued one, or a new one put in the queue.
    std::shared_ptr<TTextPropertiesImageToken> requestImage(const TTextProperties& properties, bool releaseLater);

public Q_SLOTS:
    void onCleanupCacheTimerTimeout();
    void onGrabResultReady(QSharedPointer<QQuickItemGrabResult> pCurrentGrabResult);

private:
    struct TGrabber
    {
        TOutlineTextQuickItem* item = nullptr;
        std::shared_ptr<TTextPropertiesImageToken> token;
        QSharedPointer<QQuickItemGrabResult> grabResult;
    };

    TOffscreenOutlineTextToImageService();

    /// Hands queued requests to the idle grabbers, skipping requests nobody holds
    /// any more (unless ReleaseLater), and starts their grabs.
    void processRequests();

    QTimer m_cleanupCacheTimer;
    bool m_initialized = false;
    shared::TClockService m_clockService;
    QQuickWindow* m_window = nullptr;
    std::array<TGrabber, 5> m_grabbers;
    std::deque<std::shared_ptr<TTextPropertiesImageToken>> m_requests;
    std::unordered_map<TTextProperties, std::shared_ptr<TTextPropertiesImageToken>,
                       TOutlineTextQuickItem::TTextPropertiesHash>
        m_images;
};

}
}
