#pragma once

#include "animation/phaseanimator.h"
#include "appearances/spriteinfo.h"

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace appearances {

class TAppearanceTypeSpriteInfo;

/// The pattern an instance draws: x (the direction), y and z.
struct TAppearancePattern
{
    int x = 0;
    int y = 0;
    int z = 0;
};

/// Directions in which an outfit draws its second addon before its first
/// (CIP: the type's reverse_addons_east/west/north/south flags, +0x134..0x137).
struct TReverseAddons
{
    bool east = false;
    bool west = false;
    bool north = false;
    bool south = false;
};

/// The sprites one instance draws, one per animation phase of its pattern, and
/// the animator that picks the phase (CIP appearances::TAppearanceInstanceSpriteSequence).
class TAppearanceInstanceSpriteSequence
{
public:
    /// `spritesInfo`: the frame group the instance draws; `moving`: an outfit's
    /// walking frames (their phases follow the steps, not the clock).
    TAppearanceInstanceSpriteSequence(std::shared_ptr<const TAppearanceTypeSpriteInfo> spritesInfo, bool moving);
    virtual ~TAppearanceInstanceSpriteSequence() = default;

    /// A frame group that animates gets an animator started at `startMs`.
    virtual void restartAnimation(qint64 startMs);
    /// The sprites of `pattern` for every phase (layer 0), then restarts the
    /// animation at `startMs`. Outfits pass their type's reverse addon flags.
    virtual void setPattern(const TAppearancePattern& pattern, qint64 startMs,
                            std::optional<TReverseAddons> reverseAddons = std::nullopt);
    virtual std::vector<TAppearanceInstanceSpriteInfoPtr> spriteInfos() const;
    /// The sprite of the current phase; the first without an animator; null
    /// without sprites.
    virtual const TAppearanceInstanceSpriteInfoPtr& currentSpriteInfo() const;
    virtual const TAppearanceInstanceSpriteInfoPtr& firstSpriteInfo() const;
    virtual const std::shared_ptr<animation::TAnimator>& animator() const { return m_animator; }
    /// Advances the animator to `nowMs`; true when the phase changed.
    virtual bool update(qint64 nowMs);
    /// CIP v9: a walking outfit's animator starts a step of `keyMs` at `nowMs`
    /// (nothing for any other animator).
    virtual void setAnimatorKey(qint64 keyMs, qint64 nowMs);
    /// The frame group the sequence draws.
    const std::shared_ptr<const TAppearanceTypeSpriteInfo>& typeSpritesInfo() const { return m_spritesInfo; }

    /// The sprite of `layer` for each phase of `pattern`: the sprites the frame
    /// group holds past the patterns and layers of one frame, an invalid sprite
    /// where the frame group has none.
    static std::vector<std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>>
    phaseSprites(const TAppearanceTypeSpriteInfo& spritesInfo, const TAppearancePattern& pattern, int layer);
    /// The bounding box of direction `x`; past the per-direction boxes, the
    /// first sprite's size, else 32×32.
    static QRect boundingBox(const TAppearanceTypeSpriteInfo& spritesInfo, int x);

protected:
    std::shared_ptr<const TAppearanceTypeSpriteInfo> m_spritesInfo;
    std::shared_ptr<animation::TAnimator> m_animator;
    std::vector<TAppearanceInstanceSpriteInfoPtr> m_spriteInfos;
    bool m_moving = false;
};

/// An outfit's colours and worn addons.
struct TOutfitCustomization
{
    uint8_t head = 0;
    uint8_t body = 0;
    uint8_t legs = 0;
    uint8_t feet = 0;
    bool firstAddon = false;
    bool secondAddon = false;
};

/// The sprites of an outfit with colour masks and addons: per phase the outfit
/// (masked by layer 1 with the outfit's colours when the type has two layers)
/// and the worn addons (pattern y 1 and 2), layered when more than one
/// (CIP appearances::TCustomizableOutfitAppearanceInstanceSpriteSequence).
class TCustomizableOutfitAppearanceInstanceSpriteSequence final : public TAppearanceInstanceSpriteSequence
{
public:
    using TAppearanceInstanceSpriteSequence::TAppearanceInstanceSpriteSequence;

    /// CIP v3: per phase the outfit's pattern, then the addons; the second
    /// addon first in the directions `reverseAddons` names.
    void setPattern(const TAppearancePattern& pattern, qint64 startMs,
                    std::optional<TReverseAddons> reverseAddons = std::nullopt) override;
    /// CIP v10.
    void setCustomization(const TOutfitCustomization& customization) { m_customization = customization; }
    const TOutfitCustomization& customization() const { return m_customization; }

private:
    /// The sprite info of `pattern` in `phase`, masked
    /// when the type has a colour layer.
    void appendPatternSprite(std::vector<TAppearanceInstanceSpriteInfoPtr>& layers, const TAppearancePattern& pattern,
                             size_t phase, const QRect& boundingBox) const;

    TOutfitCustomization m_customization;
};

}
}
