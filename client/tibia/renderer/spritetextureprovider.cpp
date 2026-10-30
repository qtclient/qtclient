#include "renderer/spritetextureprovider.h"

#include <QHash>
#include <QMutex>
#include <QQuickWindow>

namespace tibia::renderer {

namespace {

struct SharedProviders {
    QMutex mutex;
    QHash<QQuickWindow*, TSpriteTextureProvider*> byWindow;
};

SharedProviders& sharedProviders()
{
    static SharedProviders state;
    return state;
}

} // namespace

TSpriteTextureProvider::TSpriteTextureProvider(QQuickWindow* window, const bool allowEvict, QObject* parent)
    : cip::tibia::renderer::TSpriteTextureProvider(parent)
    , m_cache(std::make_unique<qmlcomponents::TSpriteTextureCache>(window, allowEvict))
{
}

TSpriteTextureProviderManager::TSpriteTextureProviderManager(QObject* parent)
    : cip::tibia::renderer::TSpriteTextureProviderManager(parent)
{
}

qmlcomponents::TSpriteTextureCache& TSpriteTextureProviderManager::sharedCache(QQuickWindow* window)
{
    auto& state = sharedProviders();
    const QMutexLocker lock(&state.mutex);
    if (TSpriteTextureProvider* existing = state.byWindow.value(window))
        return existing->cache();
    // CIP strong-map eviction assumes one caller requested every live key this frame.
    // Shared UI caches do not — disable eviction (0x100f78760 stays WorldMap-only).
    auto* provider = new TSpriteTextureProvider(window, false);
    state.byWindow.insert(window, provider);
    if (window) {
        QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window, [window] {
            auto& state = sharedProviders();
            const QMutexLocker lock(&state.mutex);
            delete state.byWindow.take(window);
        }, Qt::DirectConnection);
    }
    return provider->cache();
}

}
