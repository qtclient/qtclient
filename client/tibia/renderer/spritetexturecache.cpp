#include "renderer/spritetexturecache.h"

#include "appearances/spriteinfo.h"

#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QtDebug>

#include <algorithm>
#include <vector>

namespace tibia::qmlcomponents {

bool SpriteIdentifier::operator==(const SpriteIdentifier& other) const
{
    if (tint != other.tint)
        return false;
    if (!identifier || !other.identifier)
        return !identifier && !other.identifier;
    return sprites::sameSpriteIdentifier(*identifier, *other.identifier);
}

size_t qHash(const SpriteIdentifier& identifier, const size_t seed) noexcept
{
    return qHashMulti(seed, identifier.identifier ? identifier.identifier->hash() : 0, identifier.tint);
}

QSize spriteSizeOf(const SpriteIdentifier& identifier)
{
    if (!identifier.identifier || identifier.identifier->identifierKind() != sprites::ISpriteIdentifier::SpriteInfoIdentifier)
        return {};
    return static_cast<const appearances::TAppearanceInstanceSpriteInfo&>(*identifier.identifier).spriteSize();
}

TSpriteTextureCache::TSpriteTextureCache(QQuickWindow* window, const bool allowEvict)
    : m_window(window)
    , m_allowEvict(allowEvict)
{
}

// The texture creator deletes every texture once its last owner lets it go.
TSpriteTextureCache::~TSpriteTextureCache() = default;

void TSpriteTextureCache::retire(std::shared_ptr<QSGTexture> texture)
{
    if (texture)
        m_retired.append(std::move(texture));
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
    // Letting the last owners go makes the texture creator delete the
    // textures on the render thread before the next frame synchronizes.
    m_graveyard.clear();
    m_graveyard = std::move(m_retired);
    m_retired.clear();
}

void TSpriteTextureCache::clear()
{
    for (Entry& entry : m_textures) {
        retire(std::move(entry.owner));
        entry.texture = nullptr;
    }
    m_textures.clear();
    m_budgetedPixels = 0;
    m_budget = 0;
    // Two endFrame calls: this frame parks the textures, the next frame is the
    // repaint that drops node references, and only the one after that deletes.
    m_deleteHold = 2;
}

bool TSpriteTextureCache::syncHdMode(const bool hd)
{
    if (hd == m_hdMode)
        return false;
    m_hdMode = hd;
    // Callers that own the scene graph (WorldMap) must clear nodes before clear.
    // Shared UI rebuilds nodes each paint; retire frees after swap.
    clear();
    // update from this thread re-enters QML (caption items, bindings) while the
    // scene graph is rendering and parents new objects on the engine.
    if (m_window)
        QMetaObject::invokeMethod(m_window, [this] { markSceneDirty(); }, Qt::QueuedConnection);
    return true;
}

TSpriteTexture TSpriteTextureCache::lookup(const SpriteIdentifier& identifier, bool& found)
{
    // CIP TSpriteTextureCache::get: a strong hit is stamped with this frame; a weak
    // hit keeps its insert frame (that is what ages it out).
    const auto it = m_textures.find(identifier);
    found = it != m_textures.end();
    if (!found)
        return {};
    if (it->budgeted)
        it->frame = m_frame;
    return { it->texture, it->rect, it->owner };
}

void TSpriteTextureCache::insert(const SpriteIdentifier& identifier, const TSpriteTexture& texture)
{
    Entry entry{ .texture = texture.texture, .rect = texture.rect(), .frame = m_frame, .owner = texture.owner };
    // A sprite that does not exist is remembered so it is not looked up again.
    if (!entry.texture) {
        m_textures.insert(identifier, entry);
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
    m_textures.insert(identifier, entry);
}

void TSpriteTextureCache::remove(const SpriteIdentifier& identifier)
{
    // CIP: the texture is marked deleted and dropped; the budget keeps its pixels.
    const auto it = m_textures.find(identifier);
    if (it == m_textures.end())
        return;
    if (!it->budgeted && !it->texture)
        return;
    retire(it->owner);
    m_textures.erase(it);
}

void TSpriteTextureCache::removeAll()
{
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (it->budgeted || it->texture) {
            retire(it->owner);
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
                    retire(entry->owner);
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
            retire(it->owner);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
}

}
