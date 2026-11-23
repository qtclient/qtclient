#include "cachedoutlinetexttexturemanager.h"

#include <QQuickWindow>
#include <QRunnable>
#include <QSGTexture>

namespace tibia::qmlcomponents {

namespace {

/// An entry in ReleaseLater mode stays this long after it was last used.
constexpr qint64 ReleaseLaterDelayMillis = 10000;

}

TCachedOutlineTextTextureManager& TCachedOutlineTextTextureManager::instance()
{
    // Host: never destroyed, so no timer or texture is torn down after Qt's own
    // statics at exit.
    static auto* manager = new TCachedOutlineTextTextureManager();
    return *manager;
}

TCachedOutlineTextTextureManager::TCachedOutlineTextTextureManager()
    : cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager(nullptr)
    , m_clockService(0)
{
    m_cleanupTimer.setInterval(0);
    m_cleanupTimer.setSingleShot(true);
    connect(&m_cleanupTimer, &QTimer::timeout, this, &TCachedOutlineTextTextureManager::onCleanupTimerTriggered,
            Qt::QueuedConnection);
}

void TCachedOutlineTextTextureManager::attachWindow(QQuickWindow* window)
{
    if (!m_textureWindow)
        m_textureWindow = window;
    m_window = window;
    connect(window, &QQuickWindow::sceneGraphInvalidated, this,
            &TCachedOutlineTextTextureManager::onSceneGraphInvalidated, Qt::DirectConnection);
}

std::shared_ptr<QSGTexture> TCachedOutlineTextTextureManager::cachedTexture(const TTextProperties& properties)
{
    const qint64 now = m_clockService.nowMillis();
    const auto it = m_textures.find(properties);
    if (it == m_textures.end())
        return {};
    it->second.lastUsed = now;
    return it->second.texture;
}

std::shared_ptr<QSGTexture> TCachedOutlineTextTextureManager::textureForImage(const TTextProperties& properties,
                                                                            const QImage& image,
                                                                            const bool releaseLater)
{
    std::shared_ptr<QSGTexture> texture = cachedTexture(properties);
    if (texture)
        return texture;

    // Host: a window was attached before any image could be requested; without
    // one there is nothing to make the texture with.
    if (!m_textureWindow)
        return {};

    const qint64 now = m_clockService.nowMillis();
    texture.reset(m_textureWindow->createTextureFromImage(image, QQuickWindow::TextureHasAlphaChannel));
    m_textures.emplace(properties, TEntry{releaseLater, texture, now});
    return texture;
}

void TCachedOutlineTextTextureManager::onCleanupTimerTriggered()
{
    // Host: a polish can start the timer before any window was attached.
    if (!m_window)
        return;
    m_window->scheduleRenderJob(QRunnable::create([this]() { releaseUnusedTextures(); }),
                                QQuickWindow::BeforeSynchronizingStage);
}

void TCachedOutlineTextTextureManager::onSceneGraphInvalidated()
{
    m_textures.clear();
}

void TCachedOutlineTextTextureManager::releaseUnusedTextures()
{
    const qint64 now = m_clockService.nowMillis();
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        const TEntry& entry = it->second;
        if (!entry.texture || entry.texture.use_count() != 1
            || (entry.releaseLater && now < entry.lastUsed + ReleaseLaterDelayMillis)) {
            ++it;
            continue;
        }
        it = m_textures.erase(it);
    }
}

}
