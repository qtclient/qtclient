#pragma once


#include <QColor>
#include <QHash>
#include <QImage>
#include <QVector>

#include <array>
#include <cstdint>

class QQuickWindow;
class QSGTexture;

namespace tibia::qmlcomponents {

/// A sprite as the world draws it: one sprite sheet entry, an outfit sprite
/// colorized through its mask sprite, or a creature mark.
struct SpriteKey
{
    uint32_t spriteId = 0;
    /// Mask sprite to colorize with; 0 draws the sprite as is.
    uint32_t maskId = 0;
    /// Outfit color indices: head | body << 8 | legs << 16 | feet << 24.
    uint32_t colors = 0;
    /// With spriteId 0: a creature mark's frames, outer to inner, as ARGB; 0 is
    /// no frame (CIP: TCreatureMarkAppearanceInstanceSpriteInfo).
    std::array<uint32_t, 3> markColors{};
    /// Product colour map (mods effect tint) applied to the sprite; 0 draws it as is.
    uint8_t tint = 0;
    /// With spriteId 0 and no mark: CIP highlight baseplate frame 0–7
    /// (`TWorldmapHighlightBaseplateSpriteInfo`); -1 = none.
    int8_t highlightBaseplate = -1;

    bool isCreatureMark() const { return spriteId == 0 && markColors[0] != 0; }
    bool isHighlightBaseplate() const
    {
        return spriteId == 0 && markColors[0] == 0 && highlightBaseplate >= 0;
    }
    bool operator==(const SpriteKey&) const = default;
};

inline size_t qHash(const SpriteKey& key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.spriteId, key.maskId, key.colors, key.markColors[0], key.markColors[1], key.markColors[2],
                      key.tint, key.highlightBaseplate);
}

/// Sprite textures of one window, in Qt's shared texture atlas (CIP:
/// TSpriteTextureCache + TQtTextureAtlasTextureCreator). Render thread only.
///
/// Eviction (CIP: TSpriteTextureCache insert 0x100f783e0, evict 0x100f78760):
/// atlas textures count against a budget that grows with them until a texture
/// no longer fits the atlas (or, on OpenGL, past 16M pixels); then the least
/// recently used fifth of the atlas textures not drawn this frame is dropped,
/// and so are textures outside the atlas idle for 50 frames. Only caches whose
/// user calls endFrame() after requesting every texture it draws are evicted.
///
/// Shared UI caches disable eviction: other widgets may still display a key that
/// this caller did not touch this frame.
class TSpriteTextureCache
{
public:
    explicit TSpriteTextureCache(QQuickWindow* window, bool allowEvict = true);
    ~TSpriteTextureCache();

    /// The window's cache shared by its small renderers (slots, action buttons), dropped
    /// with its scene graph. Eviction is off — holders may skip texture() for idle keys.
    static TSpriteTextureCache& shared(QQuickWindow* window);

    TSpriteTextureCache(const TSpriteTextureCache&) = delete;
    TSpriteTextureCache& operator=(const TSpriteTextureCache&) = delete;

    /// Null while the sprite's sheet is still loading, or for a sprite that does not exist.
    QSGTexture* texture(const SpriteKey& key);
    /// True once texture() has stored this key, including a sprite that does not exist.
    /// False while the sheet is still loading (texture() returned null without storing it).
    bool contains(const SpriteKey& key) const { return m_textures.contains(key); }
    /// Software-renderer highlight bake: sprite RGB replaced by the highlight colour
    /// (CIP OpenGL TEffectWrapper path; unused when worldmap-highlight runs).
    QSGTexture* highlighted(const SpriteKey& key, const QColor& color);
    qsizetype size() const { return m_textures.size(); }
    /// Every texture drawn this frame was requested: evicts if an insert asked for it
    /// (and allowEvict). Retires from clear()/evict land here.
    void endFrame();
    /// Drop every texture (HD mode toggle rebuilds with/without xBR4). Callers must
    /// drop QSG nodes that referenced them first (WorldMap deletes the tree).
    void clear();

private:
    struct Entry
    {
        /// Null for a sprite that does not exist; it is not looked up again.
        QSGTexture* texture = nullptr;
        /// Frame it was last requested in.
        uint32_t frame = 0;
        uint32_t pixels = 0;
        /// Counted against the budget (CIP's strong map); the others are
        /// textures outside the atlas (its weak map).
        bool budgeted = false;
    };
    void evict(double share);
    QImage raster(const SpriteKey& key, bool& loading) const;
    void retire(QSGTexture* texture);
    void flushRetired();
    void markSceneDirty() const;

    QQuickWindow* m_window;
    struct BakedKey
    {
        SpriteKey sprite;
        QRgb color = 0;
        bool operator==(const BakedKey&) const = default;
    };
    friend size_t qHash(const BakedKey& key, size_t seed) noexcept
    {
        return qHash(key.sprite, seed ^ (static_cast<size_t>(key.color) * 0x9e3779b97f4a7c15ull));
    }
    QHash<SpriteKey, Entry> m_textures;
    QHash<BakedKey, QSGTexture*> m_baked;
    QVector<QSGTexture*> m_retired;
    QVector<QSGTexture*> m_graveyard;
    uint32_t m_frame = 0;
    quint64 m_budgetedPixels = 0;
    quint64 m_budget = 0;
    bool m_evictPending = false;
    bool m_hdMode = false;
    /// HD rebuild retires every texture while other items still hold those pointers.
    /// Skip the AfterSwap delete for this many endFrame calls so those items can
    /// repaint (see markSceneDirty) before the textures go away.
    int m_deleteHold = 0;
    /// CIP arms “atlas full” eviction only after the atlas path has accepted at least
    /// one texture. Metal/threaded GUI sync never atlases — !isAtlasTexture() is not pressure.
    bool m_seenAtlas = false;
    bool m_allowEvict = true;
};

}
