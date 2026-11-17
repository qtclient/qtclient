#pragma once

#include "animation/phaseanimator.h"
#include "appearances/spriteinfo.h"

#include <memory>
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
    /// animation at `startMs`.
    virtual void setPattern(const TAppearancePattern& pattern, qint64 startMs);
    virtual std::vector<std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>> spriteInfos() const;
    /// The sprite of the current phase; the first without an animator; null
    /// without sprites.
    virtual const std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>& currentSpriteInfo() const;
    virtual const std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>& firstSpriteInfo() const;
    virtual const std::shared_ptr<animation::TAnimator>& animator() const { return m_animator; }
    /// Advances the animator to `nowMs`; true when the phase changed.
    virtual bool update(qint64 nowMs);

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
    std::vector<std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>> m_spriteInfos;
    bool m_moving = false;
};

}
}
