#include "offscreenoutlinetexttoimageservice.h"

#include <QDebug>
#include <QQuickWindow>

namespace tibia::qmlcomponents {

namespace {

/// The cleanup starts once the cached images take this many bytes...
constexpr qsizetype CleanupThresholdBytes = 0x400000;
/// ...and releases up to this many bytes of them.
constexpr qsizetype CleanupReleaseBytes = 0x100000;

}

TOffscreenOutlineTextToImageService& TOffscreenOutlineTextToImageService::instance()
{
    // Host: never destroyed, so no timer or item is torn down after Qt's own
    // statics at exit.
    static auto* service = new TOffscreenOutlineTextToImageService();
    return *service;
}

TOffscreenOutlineTextToImageService::TOffscreenOutlineTextToImageService()
    : cip::tibia::qmlcomponents::TOffscreenOutlineTextToImageService(nullptr)
    , m_clockService(0)
{
    m_cleanupCacheTimer.setSingleShot(false);
    m_cleanupCacheTimer.setInterval(1000);
    connect(&m_cleanupCacheTimer, &QTimer::timeout, this,
            &TOffscreenOutlineTextToImageService::onCleanupCacheTimerTimeout);
    m_cleanupCacheTimer.start();
}

void TOffscreenOutlineTextToImageService::initialize(QQuickWindow* window)
{
    m_window = window;
    for (TGrabber& grabber : m_grabbers) {
        auto* item = new TOutlineTextQuickItem(nullptr);
        delete grabber.item;
        grabber.item = item;
        item->setParentItem(window->contentItem());
        item->setVisible(false);
    }
    m_initialized = true;
}

std::shared_ptr<TOffscreenOutlineTextToImageService::TTextPropertiesImageToken>
TOffscreenOutlineTextToImageService::requestImage(const TTextProperties& properties, const bool releaseLater)
{
    for (const TGrabber& grabber : m_grabbers) {
        if (grabber.token && grabber.token->properties == properties)
            return grabber.token;
    }

    const auto image = m_images.find(properties);
    if (image != m_images.end()) {
        image->second->lastRequested = m_clockService.nowMillis();
        return image->second;
    }

    for (const std::shared_ptr<TTextPropertiesImageToken>& queued : m_requests) {
        if (queued->properties == properties)
            return queued;
    }

    auto token = std::make_shared<TTextPropertiesImageToken>();
    token->properties = properties;
    token->lastRequested = m_clockService.nowMillis();
    token->releaseLater = releaseLater;
    m_requests.push_back(token);
    processRequests();
    return token;
}

void TOffscreenOutlineTextToImageService::processRequests()
{
    if (!m_initialized)
        return;

    for (TGrabber& grabber : m_grabbers) {
        if (grabber.token || grabber.grabResult)
            continue;

        std::shared_ptr<TTextPropertiesImageToken> token;
        while (!m_requests.empty()) {
            token = m_requests.front();
            m_requests.pop_front();
            // Nobody waits for it any more.
            if (token.use_count() == 1 && !token->releaseLater) {
                token.reset();
                continue;
            }
            break;
        }
        if (!token)
            return;

        grabber.token = token;
        grabber.item->setTextProperties(token->properties);
        grabber.grabResult = grabber.item->grabToImage();
        if (!grabber.grabResult) {
            // Host: nothing to grab (an empty box) would keep the grabber busy for
            // good; the text keeps its live outline text.
            grabber.token.reset();
            continue;
        }

        auto connection = std::make_shared<QMetaObject::Connection>();
        const QSharedPointer<QQuickItemGrabResult> grabResult = grabber.grabResult;
        *connection = connect(
            grabResult.data(), &QQuickItemGrabResult::ready, this,
            [this, connection, grabResult]() {
                onGrabResultReady(grabResult);
                QObject::disconnect(*connection);
            },
            Qt::QueuedConnection);
    }
}

void TOffscreenOutlineTextToImageService::onCleanupCacheTimerTimeout()
{
    qsizetype totalBytes = 0;
    for (const auto& image : m_images)
        totalBytes += image.second->image.sizeInBytes();
    if (totalBytes < CleanupThresholdBytes)
        return;

    qsizetype releasedBytes = 0;
    for (auto it = m_images.begin(); it != m_images.end();) {
        if (!it->second || it->second.use_count() != 1) {
            ++it;
            continue;
        }
        releasedBytes += it->second->image.sizeInBytes();
        if (releasedBytes > CleanupReleaseBytes)
            return;
        it = m_images.erase(it);
    }
}

void TOffscreenOutlineTextToImageService::onGrabResultReady(QSharedPointer<QQuickItemGrabResult> pCurrentGrabResult)
{
    TGrabber* finished = nullptr;
    if (pCurrentGrabResult) {
        for (TGrabber& grabber : m_grabbers) {
            if (grabber.grabResult.data() == pCurrentGrabResult.data()) {
                finished = &grabber;
                break;
            }
        }
    }

    if (!finished) {
        qDebug() << "TOffscreenOutlineTextToImageService::onGrabResultReady: grab result not found. This should not "
                    "happen";
    } else {
        const QImage image = finished->grabResult->image();
        if (finished->token && !image.isNull()) {
            finished->token->image = image;
            m_images.try_emplace(finished->token->properties, finished->token);
        }
        finished->token.reset();
        finished->grabResult.reset();
    }

    if (!m_requests.empty())
        processRequests();
}

}
