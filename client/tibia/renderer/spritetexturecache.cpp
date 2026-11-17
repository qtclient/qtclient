#include "spritetexturecache.h"
#include "tibia/runtime/host.h"

#include "renderer/spritetextureprovider.h"

#include "appearances/spriteinfo.h"
#include "appearances/worldmaphighlightbaseplate.h"
#include "mods/registry.h"
#include "sprites/sprite.h"

#include <tibia/game/gamesession.h>
#include <client/outfit.h>
#include <tibia/assets/spritesheet.h>
#include "assets/cipbmpimage.h"
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
    return tibia::assets::sprites().spriteImage(static_cast<int>(id), loading);
}

/// Outfit masks mark the head yellow, the body red, the legs green and the feet
/// blue; those pixels are multiplied by the outfit's colors.
void colorize(QImage& image, const QImage& mask, const uint32_t colors)
{
    if (mask.size() != image.size())
        return;
    const QColor head = Outfit::getColor(colors & 0xFF);
    const QColor body = Outfit::getColor((colors >> 8) & 0xFF);
    const QColor legs = Outfit::getColor((colors >> 16) & 0xFF);
    const QColor feet = Outfit::getColor(colors >> 24);
    for (int y = 0; y < image.height(); ++y) {
        uint8_t* px = image.scanLine(y);
        const uint8_t* m = mask.constScanLine(y);
        for (int x = 0; x < image.width(); ++x, px += 4, m += 4) {
            if (m[3] == 0)
                continue;
            const bool r = m[0] > 0x7F, g = m[1] > 0x7F, b = m[2] > 0x7F;
            const QColor* color = r && g && !b ? &head
                : r && !g && !b                ? &body
                : !r && g && !b                ? &legs
                : !r && !g && b                ? &feet
                                               : nullptr;
            if (!color)
                continue;
            px[0] = static_cast<uint8_t>(px[0] * color->red() / 255);
            px[1] = static_cast<uint8_t>(px[1] * color->green() / 255);
            px[2] = static_cast<uint8_t>(px[2] * color->blue() / 255);
        }
    }
}

/// TSpritesheetDecompressorThreadpool: protobuf sprite sheets are decompressed
/// off the render thread. False while the sprite's sheet is not ready; its
/// decompression is then queued.
bool sheetReady(const uint32_t spriteId)
{
    if (!tibia::game::isUsingProtobuf())
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
    , m_allowEvict(allowEvict)
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
    // update is safe from the render thread; items already synced this pass
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
        if (QImage scaled = tibia::sprites::scaleHd(image); !scaled.isNull())
            image = std::move(scaled);
    }
    return image;
}

QSGTexture* TSpriteTextureCache::texture(const SpriteKey& key)
{
    const bool hd = tibia::host::app().isHdMode();
    if (hd != m_hdMode) {
        qWarning("tibia.sprites: HD mode %s — rebuilding sprite textures", hd ? "on" : "off");
        m_hdMode = hd;
        // Callers that own the scene graph (WorldMap) must clear nodes before clear.
        // Shared UI rebuilds nodes each paint; retire frees after swap (CIP-shaped delay).
        clear();
        // update from this thread re-enters QML (caption items, bindings) while the
        // scene graph is rendering and parents new objects on the engine.
        if (m_window) {
            QMetaObject::invokeMethod(m_window, [this] { markSceneDirty(); }, Qt::QueuedConnection);
        }
    }

    bool found = false;
    if (QSGTexture* cached = lookup(key, found); found)
        return cached;

    // CIP TSpriteTextureProvider::getTexture: a sprite still loading is handed out
    // but not cached; here it is null and asked for again.
    bool loading = false;
    QImage image = raster(key, loading);
    if (loading)
        return nullptr;
    Entry entry{ .frame = m_frame };
    if (!image.isNull()) {
        // CIP TQtTextureAtlasTextureCreator : always TextureCanUseAtlas.
        entry.texture = m_window->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
        if (!entry.texture) {
            qWarning("tibia.sprites: createTextureFromImage failed (%dx%d hd=%d sprite=%u)", image.width(),
                     image.height(), hd ? 1 : 0, key.spriteId);
        }
    }
    insert(key, entry);
    return entry.texture;
}

QSGTexture* TSpriteTextureCache::lookup(const SpriteKey& key, bool& found)
{
    // CIP TSpriteTextureCache::get: a strong hit is stamped with this frame; a weak
    // hit keeps its insert frame (that is what ages it out).
    const auto it = m_textures.find(key);
    found = it != m_textures.end();
    if (!found)
        return nullptr;
    if (it->budgeted)
        it->frame = m_frame;
    return it->texture;
}

void TSpriteTextureCache::insert(const SpriteKey& key, const Entry& stored)
{
    Entry entry = stored;
    // A sprite that does not exist is remembered so it is not looked up again.
    if (!entry.texture) {
        m_textures.insert(key, entry);
        return;
    }
    const bool openGL = m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;
    const bool atlas = entry.texture->isAtlasTexture();
    const QSize size = entry.texture->textureSize();
    entry.pixels = static_cast<uint32_t>(size.width() * size.height());
    if (atlas)
        m_seenAtlas = true;
    // CIP: the budget is checked before the texture is stored, once per frame.
    if (!m_evicted) {
        if (openGL) {
            m_budget = 0x1000000;
            if (static_cast<float>(m_budgetedPixels) * 5.9604645e-08f >= 1.0f && m_allowEvict)
                evict(0.2);
        } else if (atlas) {
            m_budget = std::max(m_budget, m_budgetedPixels + entry.pixels);
        } else if (m_seenAtlas) {
            // The atlas is full. Metal/threaded GUI sync never atlases, so a texture
            // outside it is only pressure once the atlas path has been seen working.
            m_budget = m_budgetedPixels;
            if (m_allowEvict)
                evict(0.2);
        }
    }
    entry.budgeted = atlas || openGL;
    if (entry.budgeted)
        m_budgetedPixels += entry.pixels;
    m_textures.insert(key, entry);
}

void TSpriteTextureCache::remove(const SpriteKey& key)
{
    // CIP: the texture is marked deleted and dropped; the budget keeps its pixels.
    const auto it = m_textures.find(key);
    if (it == m_textures.end())
        return;
    if (!it->budgeted && !it->texture)
        return;
    retire(it->texture);
    m_textures.erase(it);
}

void TSpriteTextureCache::removeAll()
{
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (it->budgeted || it->texture) {
            retire(it->texture);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
}

void TSpriteTextureCache::beginFrame()
{
    ++m_frame;
    m_evicted = false;
}

QSGTexture* TSpriteTextureCache::highlighted(const SpriteKey& key, const QColor& color)
{
    const QRgb rgb = color.rgba();
    const BakedKey baked{ key, rgb };
    if (const auto it = m_baked.find(baked); it != m_baked.end())
        return it.value();
    bool loading = false;
    QImage image = raster(key, loading);
    if (loading || image.isNull())
        return nullptr;
    // CIP solid highlight: every texel becomes the highlight colour, its alpha the
    // sprite alpha scaled by the colour's alpha. Sprite images are RGBA8888.
    if (image.format() != QImage::Format_RGBA8888)
        image = image.convertToFormat(QImage::Format_RGBA8888);
    const float alpha = static_cast<float>(color.alphaF());
    for (int y = 0; y < image.height(); ++y) {
        uchar* px = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x, px += 4) {
            const auto a = static_cast<int>((static_cast<float>(px[3]) / 255.0f) * alpha * 255.0f);
            px[0] = static_cast<uchar>(color.red());
            px[1] = static_cast<uchar>(color.green());
            px[2] = static_cast<uchar>(color.blue());
            px[3] = static_cast<uchar>(a);
        }
    }
    QSGTexture* texture = m_window->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
    m_baked.insert(baked, texture);
    return texture;
}

void TSpriteTextureCache::endFrame()
{
    flushRetired();
    m_heldFrame = m_frame;
}

void TSpriteTextureCache::evict(const double share)
{
    if (m_evicted)
        return;
    // Strong textures no node holds, by the frame they were last drawn in. CIP asks
    // the shared_ptr use count; nodes here hold raw pointers, so a texture asked for
    // since the previous paint counts as held and is stamped with this frame.
    struct Candidate
    {
        uint32_t frame;
        uint32_t pixels;
    };
    std::vector<Candidate> idle;
    for (Entry& entry : m_textures) {
        if (!entry.texture || !entry.budgeted)
            continue;
        if (entry.frame < m_heldFrame)
            idle.push_back({ entry.frame, entry.pixels });
        else
            entry.frame = m_frame;
    }
    const auto target = static_cast<uint32_t>(static_cast<double>(m_budgetedPixels) * share);
    m_evicted = true;
    if (!idle.empty() && target != 0) {
        std::sort(idle.begin(), idle.end(), [](const Candidate& a, const Candidate& b) { return a.frame < b.frame; });
        // The frame at which the oldest textures add up to the share.
        uint32_t sum = 0;
        auto it = idle.begin();
        while (it + 1 != idle.end()) {
            sum += it->pixels;
            if (sum >= target)
                break;
            ++it;
        }
        const uint32_t oldest = it->frame;
        if (oldest != 0) {
            uint32_t dropped = 0;
            for (auto entry = m_textures.begin(); entry != m_textures.end() && dropped <= target;) {
                if (entry->texture && entry->budgeted && entry->frame <= oldest) {
                    m_budgetedPixels -= entry->pixels;
                    dropped += entry->pixels;
                    retire(entry->texture);
                    entry = m_textures.erase(entry);
                } else {
                    ++entry;
                }
            }
        }
    }
    // Textures that missed the atlas get another chance at it once there is room.
    const double used = m_budget != 0 ? static_cast<float>(m_budgetedPixels) / static_cast<float>(m_budget) : 0.0;
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (used < 0.9 && it->texture && !it->budgeted && it->frame < m_frame - 50u) {
            retire(it->texture);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
}

}
