#pragma once


#include <QColor>
#include <QHash>
#include <QImage>
#include <QVector>

#include <QRect>

#include <cstdint>
#include <memory>

class QQuickWindow;
class QSGTexture;

namespace tibia {
namespace sprites {
class ISpriteIdentifier;
}
namespace qmlcomponents {

/// What a sprite texture is asked for (CIP: the sprite identifier handed to
/// TSpriteTextureProvider; the world and UI hand it appearance instances' sprite
/// infos, identifier kind 2), with the host's mod tint.
struct SpriteIdentifier
{
    std::shared_ptr<const sprites::ISpriteIdentifier> identifier;
    /// Product colour map (mods effect tint) applied to the sprite; 0 draws it as is.
    uint8_t tint = 0;

    bool operator==(const SpriteIdentifier& other) const;
};

size_t qHash(const SpriteIdentifier& identifier, size_t seed = 0) noexcept;

/// The size of the sprite a sprite info identifier draws; empty for other identifiers.
QSize spriteSizeOf(const SpriteIdentifier& identifier);

/// A sprite's texture and the part of the sprite it holds (CIP:
/// renderer::TSpriteTexture: the texture and its rectangle in sprite pixels,
/// texture pixels divided by the sprite's scale).
struct TSpriteTexture
{
    QSGTexture* texture = nullptr;
    /// In sprite pixels; the sprite's top-left is (0, 0).
    QRect rect;
    /// The texture's owner from the texture creator (null with no texture).
    std::shared_ptr<QSGTexture> owner;
};

/// Sprite textures of one window, in Qt's shared texture atlas (CIP:
/// TSpriteTextureCache + TQtTextureAtlasTextureCreator). Render thread only.
///
/// Eviction (CIP: TSpriteTextureCache insert / evict): atlas textures go to the
/// strong map and count against a budget that grows with them until a texture
/// no longer fits the atlas (or, on OpenGL, past 16M pixels); that insert evicts
/// the least recently used fifth of the strong textures not drawn since the
/// previous endFrame, at most once per frame. Textures outside the atlas live in the weak map and
/// are dropped after 50 idle frames once the atlas has room again. Only caches
/// whose user requests every texture it draws each frame are evicted.
///
/// Shared UI caches disable eviction: other widgets may still display a key that
/// this caller did not touch this frame.
class TSpriteTextureCache
{
public:
    explicit TSpriteTextureCache(QQuickWindow* window, bool allowEvict = true);
    ~TSpriteTextureCache();

    TSpriteTextureCache(const TSpriteTextureCache&) = delete;
    TSpriteTextureCache& operator=(const TSpriteTextureCache&) = delete;

    /// CIP TSpriteTextureCache get: the stored texture of `identifier` (`found`
    /// false when none is stored). A stored null texture is a sprite that does not exist.
    TSpriteTexture lookup(const SpriteIdentifier& identifier, bool& found);
    /// CIP TSpriteTextureCache insert: stores `texture` (null: the sprite does not exist).
    void insert(const SpriteIdentifier& identifier, const TSpriteTexture& texture);
    /// True once a texture of this identifier is stored, including a sprite that does not exist.
    bool contains(const SpriteIdentifier& identifier) const { return m_textures.contains(identifier); }
    /// HD mode toggled since the last call: every texture was dropped.
    bool syncHdMode(bool hd);
    bool hdMode() const { return m_hdMode; }
    QQuickWindow* window() const { return m_window; }
    qsizetype size() const { return m_textures.size(); }
    /// The caller asked for every texture it draws. Retired textures from
    /// clear/evict/remove are deleted after the next swap.
    void endFrame();
    /// CIP onBeforeRenderNewFrame: next frame, eviction allowed again.
    void beginFrame();
    /// CIP REMOVE_SPRITEIDENTIFIER_FROM_CACHE: drop one key from either map.
    void remove(const SpriteIdentifier& identifier);
    /// A texture that is not stored: let go with the retired ones.
    void release(std::shared_ptr<QSGTexture> texture) { retire(std::move(texture)); }
    /// CIP onSceneGraphInvalidated: drop every key.
    void removeAll();
    /// Drop every texture (HD mode toggle rebuilds with/without xBR4). Callers must
    /// drop QSG nodes that referenced them first (WorldMap deletes the tree).
    void clear();

private:
    struct Entry
    {
        /// Null for a sprite that does not exist; it is not looked up again.
        QSGTexture* texture = nullptr;
        QRect rect;
        /// Frame it was last requested in.
        uint32_t frame = 0;
        uint32_t pixels = 0;
        /// Counted against the budget (CIP's strong map); the others are
        /// textures outside the atlas (its weak map).
        bool budgeted = false;
        /// The texture's owner (null for a sprite that does not exist).
        std::shared_ptr<QSGTexture> owner;
    };
    void evict(double share);
    void retire(std::shared_ptr<QSGTexture> texture);
    void flushRetired();
    void markSceneDirty() const;

    QQuickWindow* m_window;
    QHash<SpriteIdentifier, Entry> m_textures;
    QVector<std::shared_ptr<QSGTexture>> m_retired;
    QVector<std::shared_ptr<QSGTexture>> m_graveyard;
    uint32_t m_frame = 0;
    /// CIP: one eviction per frame; reset by beginFrame.
    bool m_evicted = false;
    /// Frame of the last endFrame: textures asked for since then are on screen.
    uint32_t m_heldFrame = 0;
    quint64 m_budgetedPixels = 0;
    quint64 m_budget = 0;
    bool m_hdMode = false;
    /// HD rebuild retires every texture while other items still hold those pointers.
    /// Skip the AfterSwap delete for this many endFrame calls so those items can
    /// repaint (see markSceneDirty) before the textures go away.
    int m_deleteHold = 0;
    /// CIP arms “atlas full” eviction only after the atlas path has accepted at least
    /// one texture. Metal/threaded GUI sync never atlases — !isAtlasTexture is not pressure.
    bool m_seenAtlas = false;
    bool m_allowEvict = true;
};

}
}
