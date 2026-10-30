#include "spritetexturecache.h"
#include "tibia/runtime/host.h"

#include "renderer/spritetextureprovider.h"

#include "appearances/spriteinfo.h"
#include "appearances/worldmaphighlightbaseplate.h"
#include "mods/registry.h"
#include "sprites/sprite.h"

#include <tibia/world/game.h>
#include <client/outfit.h>
#include <tibia/assets/spritesheet.h>
#include <framework/core/graphicalapplication.h>
#include <framework/graphics/image.h>
#include "sprites/hdscale.h"

#include <QImage>
#include <QMutex>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QtDebug>

#include <algorithm>
#include <vector>

namespace tibia::qmlcomponents {

namespace {

QImage spriteImage(const uint32_t id, bool& loading)
{
    const ImagePtr image = tibia::assets::sprites().spriteImage(static_cast<int>(id), loading);
    if (!image || image->getBpp() != 4)
        return {};
    const Size& size = image->getSize();
    return QImage(image->getPixelData(), size.width(), size.height(), size.width() * 4, QImage::Format_RGBA8888).copy();
}

/// Outfit masks mark the head yellow, the body red, the legs green and the feet
/// blue; those pixels are multiplied by the outfit's colors.
void colorize(QImage& image, const QImage& mask, const uint32_t colors)
{
    if (mask.size() != image.size())
        return;
    const Color head = Outfit::getColor(colors & 0xFF);
    const Color body = Outfit::getColor((colors >> 8) & 0xFF);
    const Color legs = Outfit::getColor((colors >> 16) & 0xFF);
    const Color feet = Outfit::getColor(colors >> 24);
    for (int y = 0; y < image.height(); ++y) {
        uint8_t* px = image.scanLine(y);
        const uint8_t* m = mask.constScanLine(y);
        for (int x = 0; x < image.width(); ++x, px += 4, m += 4) {
            if (m[3] == 0)
                continue;
            const bool r = m[0] > 0x7F, g = m[1] > 0x7F, b = m[2] > 0x7F;
            const Color* color = r && g && !b ? &head
                : r && !g && !b                ? &body
                : !r && g && !b                ? &legs
                : !r && !g && b                ? &feet
                                               : nullptr;
            if (!color)
                continue;
            px[0] = static_cast<uint8_t>(px[0] * color->r() / 255);
            px[1] = static_cast<uint8_t>(px[1] * color->g() / 255);
            px[2] = static_cast<uint8_t>(px[2] * color->b() / 255);
        }
    }
}

/// TSpritesheetDecompressorThreadpool: protobuf sprite sheets are decompressed
/// off the render thread. False while the sprite's sheet is not ready; its
/// decompression is then queued.
bool sheetReady(const uint32_t spriteId)
{
    if (!g_game.isUsingProtobuf())
        return true;
    // CIP sprites::TSpritesheetCache / TSpritesheetDecompressorThreadpool.
    const sprites::TSpriteInSpritesheet resolved =
        sprites::TSpritesheetCache::instance().resolve(spriteId, false);
    bool loading = false;
    const SpriteSheetPtr sheet = resolved.sheet()
        ? tibia::assets::sprites().sheetBySpriteId(static_cast<int>(spriteId), loading, false)
        : SpriteSheetPtr{};
    if (!sheet || (sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::LOADED && sheet->data))
        return true;

    static QMutex mutex;
    static QSet<const SpriteSheet*> queued;
    // Sheets that failed to load are left to the synchronous path, which gives up on the sprite.
    static QSet<const SpriteSheet*> failed;
#if !defined(__EMSCRIPTEN__)
    static QThreadPool* pool = [] {
        auto* threads = new QThreadPool;
        threads->setMaxThreadCount(std::max(1, QThread::idealThreadCount() / 2));
        return threads;
    }();
#endif
    const QMutexLocker lock(&mutex);
    if (failed.contains(sheet.get()))
        return true;
    if (queued.contains(sheet.get()))
        return false;
#if defined(__EMSCRIPTEN__)
    // The browser build's extra QThreadPool worker aborts the runtime (the
    // start screen dies as soon as the boosted creature asks for a sheet).
    // Decompress on this thread; the sheet is one LZMA blob.
    const bool loaded = tibia::assets::sprites().loadSheet(sheet);
    if (!loaded && sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::NONE)
        failed.insert(sheet.get());
    return true;
#else
    queued.insert(sheet.get());
    pool->start([sheet] {
        const bool loaded = tibia::assets::sprites().loadSheet(sheet);
        const QMutexLocker lock(&mutex);
        queued.remove(sheet.get());
        if (!loaded && sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::NONE)
            failed.insert(sheet.get());
    });
    return false;
#endif
}

}

TSpriteTextureCache::TSpriteTextureCache(QQuickWindow* window, const bool allowEvict)
    : m_window(window)
    , m_allowEvict(sprites::TQtTextureAtlasTextureCreator(allowEvict).allowEvict())
{
}

TSpriteTextureCache::~TSpriteTextureCache()
{
    for (const Entry& entry : std::as_const(m_textures))
        delete entry.texture;
    for (QSGTexture* texture : std::as_const(m_retired))
        delete texture;
    for (QSGTexture* texture : std::as_const(m_graveyard))
        delete texture;
}

TSpriteTextureCache& TSpriteTextureCache::shared(QQuickWindow* window)
{
    // CIP: TSpriteTextureProviderManager owns the shared UI providers; cache is the
    // backing store (no parallel keep-alive map beside the manager).
    return ::tibia::renderer::TSpriteTextureProviderManager::sharedCache(window);
}

void TSpriteTextureCache::retire(QSGTexture* texture)
{
    if (texture)
        m_retired.append(texture);
}

void TSpriteTextureCache::markSceneDirty() const
{
    QQuickItem* root = m_window ? m_window->contentItem() : nullptr;
    if (!root)
        return;
    // Every sprite node has to drop the retired textures on its next paint.
    // update() is safe from the render thread; items already synced this pass
    // pick it up on the following frame, which is why deletion waits.
    const auto visit = [](auto&& self, QQuickItem* item) -> void {
        item->update();
        const QList<QQuickItem*> children = item->childItems();
        for (QQuickItem* child : children)
            self(self, child);
    };
    visit(visit, root);
}

void TSpriteTextureCache::flushRetired()
{
    if (m_deleteHold > 0) {
        --m_deleteHold;
        if (!m_retired.isEmpty()) {
            m_graveyard += std::move(m_retired);
            m_retired.clear();
        }
        return;
    }
    if (!m_graveyard.isEmpty()) {
        if (m_window) {
            struct DeleteTextures final : QRunnable
            {
                explicit DeleteTextures(QVector<QSGTexture*> textures)
                    : m_textures(std::move(textures))
                {
                    setAutoDelete(true);
                }
                void run() override
                {
                    for (QSGTexture* texture : m_textures)
                        delete texture;
                }
                QVector<QSGTexture*> m_textures;
            };
            // AfterSwap: Metal may still sample the previous frame until present completes.
            m_window->scheduleRenderJob(new DeleteTextures(std::move(m_graveyard)), QQuickWindow::AfterSwapStage);
        } else {
            for (QSGTexture* texture : std::as_const(m_graveyard))
                delete texture;
        }
        m_graveyard.clear();
    }
    m_graveyard = std::move(m_retired);
    m_retired.clear();
}

void TSpriteTextureCache::clear()
{
    for (QSGTexture* texture : std::as_const(m_baked))
        retire(texture);
    m_baked.clear();
    for (Entry& entry : m_textures) {
        retire(entry.texture);
        entry.texture = nullptr;
    }
    m_textures.clear();
    m_budgetedPixels = 0;
    m_budget = 0;
    m_evictPending = false;
    // Two endFrame calls: this frame parks the textures, the next frame is the
    // repaint that drops node references, and only the one after that deletes.
    m_deleteHold = 2;
}

QImage TSpriteTextureCache::raster(const SpriteKey& key, bool& loading) const
{
    loading = false;
    QImage image;
    if (key.isCreatureMark()) {
        image = appearances::TCreatureMarkAppearanceInstanceSpriteInfo::imageForColors(key.markColors);
    } else if (key.isHighlightBaseplate()) {
        image = appearances::TWorldmapHighlightBaseplateSpriteInfo::imageForFrame(
            static_cast<uint8_t>(key.highlightBaseplate));
    } else {
        // Not stored: a sheet still decompressing must be asked for again. Caching
        // the miss paints a hole until something clears the texture cache.
        if (!sheetReady(key.spriteId) || (key.maskId && !sheetReady(key.maskId))) {
            loading = true;
            return {};
        }
        image = spriteImage(key.spriteId, loading);
        if (loading)
            return {};
    }
    if (!image.isNull() && key.maskId) {
        const QImage mask = spriteImage(key.maskId, loading);
        if (loading)
            return {};
        colorize(image, mask, key.colors);
    }
    if (!image.isNull() && key.tint) {
        if (const auto apply = tibia::mods::applyImageTintHook())
            apply(image, key.tint);
    }
    // Product HD scaler (product registers xBRZ 4× on tibia::sprites::setHdScaler).
    // Creature marks stay native (vector frames); draw path divides HD textures by factor.
    if (m_hdMode && !image.isNull() && !key.isCreatureMark() && !key.isHighlightBaseplate()
        && tibia::sprites::hdFactor() > 1) {
        const ImagePtr src = std::make_shared<Image>(Size(image.width(), image.height()), 4, image.constBits());
        if (const ImagePtr scaled = tibia::sprites::scaleHd(src); scaled && scaled != src) {
            image = QImage(scaled->getPixelData(), scaled->getWidth(), scaled->getHeight(), scaled->getWidth() * 4,
                           QImage::Format_RGBA8888)
                        .copy();
        }
    }
    return image;
}

QSGTexture* TSpriteTextureCache::texture(const SpriteKey& key)
{
    const bool hd = tibia::host::app().isHdMode();
    if (hd != m_hdMode) {
        qWarning("tibia.sprites: HD mode %s — rebuilding sprite textures", hd ? "on" : "off");
        m_hdMode = hd;
        // Callers that own the scene graph (WorldMap) must clear nodes before clear().
        // Shared UI rebuilds nodes each paint; retire frees after swap (CIP-shaped delay).
        clear();
        // update() from this thread re-enters QML (caption items, bindings) while the
        // scene graph is rendering and parents new objects on the engine.
        if (m_window) {
            QMetaObject::invokeMethod(m_window, [this] { markSceneDirty(); }, Qt::QueuedConnection);
        }
    }

    if (const auto it = m_textures.find(key); it != m_textures.end()) {
        it->frame = m_frame;
        return it->texture;
    }

    bool loading = false;
    QImage image = raster(key, loading);
    if (loading)
        return nullptr;
    Entry entry{ .frame = m_frame };
    if (!image.isNull()) {
        // CIP TQtTextureAtlasTextureCreator (0x100ff2050): always TextureCanUseAtlas.
        entry.texture = m_window->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
        if (!entry.texture) {
            qWarning("tibia.sprites: createTextureFromImage failed (%dx%d hd=%d sprite=%u)", image.width(),
                     image.height(), hd ? 1 : 0, key.spriteId);
        } else {
            entry.pixels = static_cast<uint32_t>(image.width() * image.height());
            const bool openGL = m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;
            const bool atlas = entry.texture->isAtlasTexture();
            // CIP insert 0x100f783e0: OpenGL → 16M strong budget; atlas → grow strong budget;
            // non-atlas on a live atlas path → “full”, evict, weak map. Metal/threaded GUI
            // sync never atlases — !atlas alone is not CIP pressure (m_seenAtlas gate).
            if (atlas)
                m_seenAtlas = true;
            if (openGL) {
                m_budget = 0x1000000;
                if (m_budgetedPixels >= m_budget)
                    m_evictPending = true;
                entry.budgeted = true;
            } else if (atlas) {
                m_budget = std::max(m_budget, m_budgetedPixels + entry.pixels);
                entry.budgeted = true;
            } else if (m_seenAtlas) {
                m_budget = m_budgetedPixels;
                m_evictPending = true;
                entry.budgeted = false;
            } else {
                entry.budgeted = false;
            }
            if (entry.budgeted)
                m_budgetedPixels += entry.pixels;
        }
    }
    m_textures.insert(key, entry);
    return entry.texture;
}

QSGTexture* TSpriteTextureCache::highlighted(const SpriteKey& key, const QColor& color)
{
    const QRgb rgb = qRgb(color.red(), color.green(), color.blue());
    const BakedKey baked{ key, rgb };
    if (const auto it = m_baked.find(baked); it != m_baked.end())
        return it.value();
    bool loading = false;
    QImage image = raster(key, loading);
    if (loading || image.isNull())
        return nullptr;
    // Full-alpha mix: the texel becomes the highlight colour, masked by the sprite alpha.
    for (int y = 0; y < image.height(); ++y) {
        uchar* px = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x, px += 4) {
            if (px[3] == 0)
                continue;
            px[0] = static_cast<uchar>(color.red());
            px[1] = static_cast<uchar>(color.green());
            px[2] = static_cast<uchar>(color.blue());
        }
    }
    QSGTexture* texture = m_window->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
    m_baked.insert(baked, texture);
    return texture;
}

void TSpriteTextureCache::endFrame()
{
    flushRetired();
    if (m_allowEvict && m_evictPending) {
        m_evictPending = false;
        evict(0.2);
    } else {
        m_evictPending = false;
    }
    ++m_frame;
}

void TSpriteTextureCache::evict(const double share)
{
    struct Candidate
    {
        uint32_t frame;
        uint32_t pixels;
    };
    std::vector<Candidate> idle;
    for (const Entry& entry : std::as_const(m_textures)) {
        if (entry.texture && entry.budgeted && entry.frame != m_frame)
            idle.push_back({ entry.frame, entry.pixels });
    }
    const auto target = static_cast<quint64>(m_budgetedPixels * share);
    if (!idle.empty() && target > 0) {
        std::sort(idle.begin(), idle.end(), [](const Candidate& a, const Candidate& b) { return a.frame < b.frame; });
        // The oldest frame whose textures, with the older ones, make up the share.
        quint64 sum = 0;
        uint32_t oldest = idle.back().frame;
        for (const Candidate& candidate : idle) {
            sum += candidate.pixels;
            oldest = candidate.frame;
            if (sum >= target)
                break;
        }
        quint64 dropped = 0;
        for (auto it = m_textures.begin(); it != m_textures.end() && dropped <= target;) {
            if (it->texture && it->budgeted && it->frame != m_frame && it->frame <= oldest) {
                dropped += it->pixels;
                m_budgetedPixels -= it->pixels;
                retire(it->texture);
                it = m_textures.erase(it);
            } else {
                ++it;
            }
        }
    }
    // Textures that missed the atlas get another chance at it once there is room.
    if (m_budget > 0 && static_cast<double>(m_budgetedPixels) / m_budget < 0.9) {
        for (auto it = m_textures.begin(); it != m_textures.end();) {
            if (it->texture && !it->budgeted && it->frame + 50 < m_frame) {
                retire(it->texture);
                it = m_textures.erase(it);
            } else {
                ++it;
            }
        }
    }
}

}
