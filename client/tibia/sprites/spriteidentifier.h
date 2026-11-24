#pragma once

#include "sprites/spritefromspritesheetinfo.h"

#include <QColor>

#include <cstddef>
#include <memory>

namespace tibia {
namespace sprites {

/// What the sprite resolver turns into a sprite and texture providers cache by
/// (CIP sprites::ISpriteIdentifier: v2 kind, v4 equals, v5 hash).
class ISpriteIdentifier
{
public:
    /// CIP v2 values.
    enum EIdentifierKind
    {
        ResourceIdentifier = 1,
        SpriteInfoIdentifier = 2,
        AssetSpritesheetIdentifier = 3,
        MinimapTileIdentifier = 4,
        EffectWrapperIdentifier = 5,
    };

    virtual ~ISpriteIdentifier() = default;
    /// CIP v2.
    virtual int identifierKind() const = 0;
    /// CIP v3: a copy of this identifier.
    virtual std::shared_ptr<ISpriteIdentifier> clone() const = 0;
    /// CIP v4: callers compare only identifiers of the same dynamic type and kind.
    virtual bool equals(const ISpriteIdentifier& other) const = 0;
    /// CIP v5.
    virtual size_t hash() const = 0;
};

using ISpriteIdentifierPtr = std::shared_ptr<const ISpriteIdentifier>;

/// Two identifiers name the same sprite: same dynamic type and kind, and equal.
bool sameSpriteIdentifier(const ISpriteIdentifier& a, const ISpriteIdentifier& b);

/// boost::hash_combine (64 bit) as CipSoft's identifier hashes use it.
size_t hashCombine(size_t seed, size_t value);
/// The hash identifiers are kept by: kind and hash() combined, cut to 32 bits.
size_t identifierHash(const ISpriteIdentifier& identifier);

/// A whole asset spritesheet as one sprite (CIP sprites::TAssetSpritesheetIdentifier).
class TAssetSpritesheetIdentifier final : public ISpriteIdentifier
{
public:
    explicit TAssetSpritesheetIdentifier(TSpritesheetID spritesheetId, float scale = 1.f)
        : m_spritesheetId(std::move(spritesheetId))
        , m_scale(scale)
    {
    }

    int identifierKind() const override { return AssetSpritesheetIdentifier; }
    std::shared_ptr<ISpriteIdentifier> clone() const override;
    bool equals(const ISpriteIdentifier& other) const override;
    size_t hash() const override;

    const TSpritesheetID& spritesheetId() const { return m_spritesheetId; }
    float scale() const { return m_scale; }

private:
    TSpritesheetID m_spritesheetId;
    float m_scale = 1.f;
};

/// Another identifier's sprite with an image effect applied (CIP
/// sprites::TEffectWrapperSpritesheetIdentifier).
class TEffectWrapperSpritesheetIdentifier final : public ISpriteIdentifier
{
public:
    enum class EEffect
    {
        /// Each colour channel half way to the effect colour, alpha scaled by its alpha.
        Blend = 0,
        /// Every pixel the effect colour, alpha scaled by its alpha.
        Highlight = 1,
    };

    /// CIP clones `wrapped`; identifiers are immutable here, so it is shared.
    TEffectWrapperSpritesheetIdentifier(ISpriteIdentifierPtr wrapped, EEffect effect, const QColor& color)
        : m_wrapped(std::move(wrapped))
        , m_effect(effect)
        , m_color(color)
    {
    }

    int identifierKind() const override { return EffectWrapperIdentifier; }
    /// A copy holding a copy of the wrapped identifier.
    std::shared_ptr<ISpriteIdentifier> clone() const override;
    bool equals(const ISpriteIdentifier& other) const override;
    size_t hash() const override;

    const ISpriteIdentifierPtr& wrapped() const { return m_wrapped; }
    EEffect effect() const { return m_effect; }
    const QColor& color() const { return m_color; }

private:
    ISpriteIdentifierPtr m_wrapped;
    EEffect m_effect = EEffect::Blend;
    QColor m_color;
};

}
}
