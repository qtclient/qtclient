#pragma once

#include "renderer/spritetexturecache.h"

#include <cip_gen.h>

#include <QImage>

#include <memory>

class QQuickWindow;

namespace tibia {
namespace sprites {
class TSprite;
}
namespace renderer {

using qmlcomponents::SpriteIdentifier;
using qmlcomponents::TSpriteTexture;

/// CIP TSpriteTextureProvider: the textures of one window's sprites, made from
/// what the sprite resolver resolves an identifier to and kept in a
/// TSpriteTextureCache.
class TSpriteTextureProvider final : public cip::tibia::renderer::TSpriteTextureProvider
{
    Q_OBJECT
public:
    explicit TSpriteTextureProvider(QQuickWindow* window, bool allowEvict = true, QObject* parent = nullptr);

    /// CIP v12: the texture of `identifier`'s sprite, scaled `scale` times with
    /// `mode` (1 keeps the sprite's size). Unless `bypassCache`, a stored texture
    /// is handed out and a new cacheable one stored. Null texture while the
    /// sprite's spritesheet is not decompressed yet; a sprite that does not exist
    /// is stored as a null texture (host: so it is not waited on).
    TSpriteTexture texture(const SpriteIdentifier& identifier, uint8_t scale, Qt::TransformationMode mode,
                           bool bypassCache);
    /// CIP v13: texture with scale 1, fast transformation, cached.
    TSpriteTexture texture(const SpriteIdentifier& identifier)
    {
        return texture(identifier, 1, Qt::FastTransformation, false);
    }
    /// The texture of `identifier` with every pixel in `color` (software renderer
    /// highlight); null until the sprite is resolved.
    QSGTexture* highlightedTexture(const SpriteIdentifier& identifier, const QColor& color);

    /// The image a texture is made of (CIP: the provider's image helper): the
    /// sprite's image cut to `rect`, or for sprites of resources cut to the
    /// pixels that are not fully transparent (then `rect` becomes that part),
    /// scaled `scale` times when above 1.
    static QImage textureImage(const sprites::TSprite& sprite, QRect& rect, uint8_t scale, Qt::TransformationMode mode);
    /// The part of `image` whose pixels are not fully transparent; `rect` is
    /// updated to it (its size bounds the left and top edge from above). The
    /// whole image when every pixel is transparent.
    static QImage trimmedToOpaquePixels(const QImage& image, QRect& rect);

    qmlcomponents::TSpriteTextureCache& cache() { return *m_cache; }
    const qmlcomponents::TSpriteTextureCache& cache() const { return *m_cache; }

public Q_SLOTS:
    /// REMOVE_SPRITEIDENTIFIER_FROM_CACHE: the cached texture of the action's
    /// sprite identifier is dropped.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void onBeforeRenderNewFrame();
    void onSceneGraphInvalidated();

private:
    /// The sprite resolved for `identifier` as the texture's image (host: mod
    /// tint and the HD scaler come after CIP's cut and scale). Null image with
    /// `loading` while a spritesheet is not decompressed yet.
    QImage resolvedImage(const SpriteIdentifier& identifier, uint8_t scale, Qt::TransformationMode mode, QRect& rect,
                         bool& loading, bool& cacheable) const;

    QQuickWindow* m_window = nullptr;
    std::unique_ptr<qmlcomponents::TSpriteTextureCache> m_cache;
};

/// Shared UI texture providers per window (CIP: TSpriteTextureProviderManager).
class TSpriteTextureProviderManager final : public cip::tibia::renderer::TSpriteTextureProviderManager
{
    Q_OBJECT
public:
    explicit TSpriteTextureProviderManager(QObject* parent = nullptr);

    /// The window's provider shared by its small renderers (slots, action buttons),
    /// dropped with its scene graph. Eviction is off: holders may skip asking for
    /// idle sprites.
    static TSpriteTextureProvider& sharedProvider(QQuickWindow* window);
    /// REMOVE_SPRITEIDENTIFIER_FROM_CACHE goes to every provider. Host: the
    /// providers live in a process-wide registry, so this is static.
    static void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
};

/// CIP: the highlight pulse of the frame at `Timestamp` (milliseconds); the
/// game client sets it before each frame.
void updateHighlightStrength(qint64 Timestamp);
/// The highlight pulse the current frame's highlights are drawn with.
float highlightStrength();

}
}
