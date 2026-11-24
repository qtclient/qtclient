#include "renderer/spritetextureprovider.h"

#include "appearances/spriteinfo.h"
#include "input/gameaction.h"
#include "mods/registry.h"
#include "sprites/hdscale.h"
#include "sprites/sprite.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesystem.h"
#include "tibia/runtime/host.h"

#include <QHash>
#include <QMutex>
#include <QQuickWindow>
#include <QSGTexture>

#include <cmath>
#include <utility>

namespace tibia::renderer {

namespace {

/// The game client's manager the UI renderers share providers through.
TSpriteTextureProviderManager* g_liveManager = nullptr;

bool spritesExist(const sprites::ISpriteIdentifier& identifier);

/// Host: the spritesheets `info` draws from exist (a resolved null sprite then
/// means a spritesheet is still loading).
bool spriteInfoSpritesExist(const appearances::TAppearanceInstanceSpriteInfo& info)
{
    using EKind = appearances::TAppearanceInstanceSpriteInfo::EKind;
    const auto exists = [](const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite) {
        return sprite && !sprite->spritesheetId.isEmpty();
    };
    switch (info.kind()) {
    case EKind::Spritesheet:
        return exists(static_cast<const appearances::TSpritesheetAppearanceInstanceSpriteInfo&>(info).sprite());
    case EKind::Masked: {
        const auto& masked = static_cast<const appearances::TMaskedAppearanceInstanceSpriteInfo&>(info);
        return exists(masked.sprite()) && exists(masked.mask());
    }
    case EKind::Layered:
        for (const auto& layer : static_cast<const appearances::TLayeredAppearanceInstanceSpriteInfo&>(info).layers()) {
            if (!layer || !spriteInfoSpritesExist(*layer))
                return false;
        }
        return true;
    case EKind::Dummy:
        return false;
    default:
        return true;
    }
}

/// Host: the spritesheets `identifier` draws from exist.
bool spritesExist(const sprites::ISpriteIdentifier& identifier)
{
    switch (identifier.identifierKind()) {
    case sprites::ISpriteIdentifier::SpriteInfoIdentifier:
        return spriteInfoSpritesExist(static_cast<const appearances::TAppearanceInstanceSpriteInfo&>(identifier));
    case sprites::ISpriteIdentifier::EffectWrapperIdentifier: {
        const auto& wrapped = static_cast<const sprites::TEffectWrapperSpritesheetIdentifier&>(identifier).wrapped();
        return wrapped && spritesExist(*wrapped);
    }
    default:
        return true;
    }
}

} // namespace

TSpriteTextureProvider::TSpriteTextureProvider(QQuickWindow* window, const bool allowEvict, QObject* parent)
    : cip::tibia::renderer::TSpriteTextureProvider(parent)
    , m_window(window)
    , m_cache(std::make_unique<qmlcomponents::TSpriteTextureCache>(window, allowEvict))
{
    if (!window)
        return;
    m_textureCreator = std::make_unique<sprites::TQtTextureAtlasTextureCreator>(window);
    // Both fire on the render thread, where the cache lives.
    connect(window, &QQuickWindow::beforeRendering, this, &TSpriteTextureProvider::onBeforeRenderNewFrame,
            Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &TSpriteTextureProvider::onSceneGraphInvalidated,
            Qt::DirectConnection);
}

TSpriteTextureProvider::~TSpriteTextureProvider()
{
    // The cache deletes the empty texture with its own.
    if (m_emptyTexture.texture)
        m_cache->release(m_emptyTexture.owner);
}

QImage TSpriteTextureProvider::trimmedToOpaquePixels(const QImage& image, QRect& rect)
{
    int left = rect.width();
    int top = rect.height();
    int right = 0;
    int bottom = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const uint32_t*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (line[x] > 0xffffffu) {
                right = std::max(right, x);
                left = std::min(left, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < left || bottom < top)
        return image;
    rect = QRect(QPoint(left, top), QPoint(right, bottom));
    return image.copy(rect);
}

QImage TSpriteTextureProvider::textureImage(const sprites::TSprite& sprite, QRect& rect, const uint8_t scale,
                                            const Qt::TransformationMode mode)
{
    const QImage source = sprite.getImage();
    QImage image = sprite.isFromResource() ? trimmedToOpaquePixels(source, rect) : source.copy(rect);
    if (scale > 1) {
        const QSize size(static_cast<int>(std::round(scale * static_cast<double>(image.width()))),
                         static_cast<int>(std::round(scale * static_cast<double>(image.height()))));
        image = image.scaled(size, Qt::KeepAspectRatio, mode);
    }
    return image;
}

QImage TSpriteTextureProvider::resolvedImage(const SpriteIdentifier& identifier, const uint8_t scale,
                                             const Qt::TransformationMode mode, QRect& rect, bool& loading,
                                             bool& cacheable) const
{
    loading = false;
    cacheable = true;
    if (!identifier.identifier)
        return {};
    const sprites::ISpriteIdentifier& resolved = *identifier.identifier;
    const std::shared_ptr<sprites::TSprite> sprite = sprites::TSpriteSystem::instance().resolver().resolve(resolved);
    if (!sprite) {
        loading = spritesExist(resolved);
        return {};
    }
    const auto* info = resolved.identifierKind() == sprites::ISpriteIdentifier::SpriteInfoIdentifier
        ? static_cast<const appearances::TAppearanceInstanceSpriteInfo*>(&resolved)
        : nullptr;
    rect = QRect(QPoint(0, 0), sprite->getSize());
    if (info)
        rect = info->boundingBox();
    QImage image = textureImage(*sprite, rect, scale, mode);
    cacheable = sprite->isCacheable() && !sprite->isHighlighted();
    if (image.isNull())
        return image;
    // Host: the pack's effect tint, then its HD scaler (the texture then holds
    // hdFactor texels per sprite pixel; creature marks and the baseplate stay native).
    if (identifier.tint) {
        if (const auto apply = tibia::mods::applyImageTintHook()) {
            image = image.convertToFormat(QImage::Format_RGBA8888);
            apply(image, identifier.tint);
        }
    }
    using EKind = appearances::TAppearanceInstanceSpriteInfo::EKind;
    const bool native = info && (info->kind() == EKind::CreatureMark || info->kind() == EKind::HighlightBaseplate);
    if (m_cache->hdMode() && !native && tibia::sprites::hdFactor() > 1) {
        if (QImage scaled = tibia::sprites::scaleHd(image); !scaled.isNull())
            image = std::move(scaled);
    }
    return image;
}

TSpriteTexture TSpriteTextureProvider::texture(const SpriteIdentifier& identifier, const uint8_t scale,
                                               const Qt::TransformationMode mode, const bool bypassCache)
{
    m_cache->syncHdMode(tibia::host::app().isHdMode());
    if (!bypassCache) {
        bool found = false;
        if (const TSpriteTexture cached = m_cache->lookup(identifier, found); found)
            return cached;
    }
    QRect rect;
    bool loading = false;
    bool cacheable = true;
    const QImage image = resolvedImage(identifier, scale, mode, rect, loading, cacheable);
    if (loading)
        return {};
    TSpriteTexture texture{ .m_rect = rect };
    if (!image.isNull() && m_textureCreator) {
        // Cacheable sprites may go to the atlas.
        texture.owner = cacheable ? m_textureCreator->createTextureFromImage(image)
                                  : m_textureCreator->createNonAtlasTextureFromImage(image);
        texture.texture = texture.owner.get();
    }
    if (!bypassCache && cacheable)
        m_cache->insert(identifier, texture);
    else
        m_cache->release(texture.owner);
    return texture;
}

QSGTexture* TSpriteTextureProvider::highlightedTexture(const SpriteIdentifier& identifier, const QColor& color)
{
    // CIP's OpenGL path: the sprite wrapped in a highlight effect.
    const SpriteIdentifier highlighted{ std::make_shared<sprites::TEffectWrapperSpritesheetIdentifier>(
                                            identifier.identifier,
                                            sprites::TEffectWrapperSpritesheetIdentifier::EEffect::Highlight, color),
                                        0 };
    return texture(highlighted).texture;
}

void TSpriteTextureProvider::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (pGameAction->gameActionType() != input::EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE)
        return;
    const auto* remove = input::gameActionAs<input::TGameActionRemoveSpriteIdentifierFromCache>(pGameAction);
    if (!remove)
        return;
    m_cache->remove(SpriteIdentifier{ remove->spriteIdentifier(), 0 });
}

void TSpriteTextureProvider::onBeforeRenderNewFrame()
{
    m_cache->beginFrame();
}

TSpriteTexture TSpriteTextureProvider::emptyTexture()
{
    if (!m_emptyTexture.texture && m_textureCreator) {
        m_emptyTexture.owner = m_textureCreator->createTextureFromImage(QImage());
        m_emptyTexture.texture = m_emptyTexture.owner.get();
    }
    return m_emptyTexture;
}

void TSpriteTextureProvider::onSceneGraphInvalidated()
{
    m_cache->removeAll();
    // Host: the empty texture belongs to the scene graph going away.
    if (m_emptyTexture.texture)
        m_cache->release(m_emptyTexture.owner);
    m_emptyTexture = {};
}

TSpriteTextureProviderManager::TSpriteTextureProviderManager(std::shared_ptr<sprites::TSpriteResolver> spriteResolver,
                                                             QObject* parent)
    : cip::tibia::renderer::TSpriteTextureProviderManager(parent)
    , m_spriteResolver(std::move(spriteResolver))
{
    g_liveManager = this;
}

TSpriteTextureProviderManager::~TSpriteTextureProviderManager()
{
    if (g_liveManager == this)
        g_liveManager = nullptr;
    // Host: a provider still in the map belongs to a live scene graph and
    // holds its textures; it is left to it.
}

TSpriteTextureProvider& TSpriteTextureProviderManager::sharedProvider(QQuickWindow* window)
{
    if (g_liveManager)
        return g_liveManager->provider(window);
    // Host: renderers outside a game client (tools, tests) share a private
    // manager that is never destroyed.
    static TSpriteTextureProviderManager* const fallback = [] {
        auto* manager = new TSpriteTextureProviderManager(nullptr);
        g_liveManager = nullptr;
        return manager;
    }();
    return fallback->provider(window);
}

TSpriteTextureProvider& TSpriteTextureProviderManager::provider(QQuickWindow* window)
{
    const QMutexLocker lock(&m_mutex);
    if (TSpriteTextureProvider* existing = m_providers.value(window))
        return *existing;
    // CIP strong-map eviction assumes one caller requested every live key this frame.
    // Shared UI caches do not — disable eviction (stays WorldMap-only).
    auto* provider = new TSpriteTextureProvider(window, false);
    m_providers.insert(window, provider);
    if (window) {
        QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, this, [this, window] {
            const QMutexLocker lock(&m_mutex);
            delete m_providers.take(window);
        }, Qt::DirectConnection);
    }
    return *provider;
}

void TSpriteTextureProviderManager::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (pGameAction->gameActionType() != input::EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE)
        return;
    const QMutexLocker lock(&m_mutex);
    for (TSpriteTextureProvider* provider : std::as_const(m_providers))
        provider->handleGameAction(pGameAction);
}

namespace {

float g_highlightStrength = 0.0f;

}

void updateHighlightStrength(const qint64 Timestamp)
{
    g_highlightStrength =
        static_cast<float>((std::sin(static_cast<double>(Timestamp) / 100.0) * 0.15f + 0.65f) * 0.75);
}

float highlightStrength()
{
    return g_highlightStrength;
}

}
