#pragma once

#include "renderer/spritetexturecache.h"

#include <cip_gen.h>

#include <memory>

class QQuickWindow;

namespace tibia::renderer {

/// CIP TSpriteTextureProvider: textures for one window's world/UI sprites.
/// Backed by qmlcomponents::TSpriteTextureCache.
class TSpriteTextureProvider final : public cip::tibia::renderer::TSpriteTextureProvider
{
    Q_OBJECT
public:
    explicit TSpriteTextureProvider(QQuickWindow* window, bool allowEvict = true, QObject* parent = nullptr);

    qmlcomponents::TSpriteTextureCache& cache() { return *m_cache; }
    const qmlcomponents::TSpriteTextureCache& cache() const { return *m_cache; }

private:
    std::unique_ptr<qmlcomponents::TSpriteTextureCache> m_cache;
};

/// Shared UI texture caches per window (CIP: TSpriteTextureProviderManager).
class TSpriteTextureProviderManager final : public cip::tibia::renderer::TSpriteTextureProviderManager
{
    Q_OBJECT
public:
    explicit TSpriteTextureProviderManager(QObject* parent = nullptr);

    /// Window-shared cache (eviction off) — same as TSpriteTextureCache::shared.
    static qmlcomponents::TSpriteTextureCache& sharedCache(QQuickWindow* window);
};

}
