#include "appearances/appearanceinstancespritesequence.h"

#include "animation/movingoutfitanimator.h"
#include "appearances/appearancetype.h"
#include "sprites/spritesystem.h"

namespace tibia::appearances {

TAppearanceInstanceSpriteSequence::TAppearanceInstanceSpriteSequence(std::shared_ptr<const TAppearanceTypeSpriteInfo> spritesInfo,
                                                                     const bool moving)
    : m_spritesInfo(std::move(spritesInfo))
    , m_moving(moving)
{
}

void TAppearanceInstanceSpriteSequence::restartAnimation(const qint64 startMs)
{
    // Walking frames are animated by the creature's steps (host: TCreature).
    if (!m_spritesInfo || m_moving)
        return;
    if (const animation::TAnimationInfo* info = m_spritesInfo->animation())
        m_animator = std::make_shared<animation::TAnimator>(*info, startMs);
}

void TAppearanceInstanceSpriteSequence::setPattern(const TAppearancePattern& pattern, const qint64 startMs,
                                                   std::optional<TReverseAddons>)
{
    m_spriteInfos.clear();
    if (m_spritesInfo) {
        const QRect box = boundingBox(*m_spritesInfo, pattern.x);
        for (auto& sprite : phaseSprites(*m_spritesInfo, pattern, 0))
            m_spriteInfos.push_back(std::make_shared<TSpritesheetAppearanceInstanceSpriteInfo>(std::move(sprite), box));
    }
    restartAnimation(startMs);
}

std::vector<TAppearanceInstanceSpriteInfoPtr> TAppearanceInstanceSpriteSequence::spriteInfos() const
{
    return m_spriteInfos;
}

const TAppearanceInstanceSpriteInfoPtr& TAppearanceInstanceSpriteSequence::currentSpriteInfo() const
{
    if (m_animator) {
        const auto phase = static_cast<size_t>(m_animator->phase());
        if (phase < m_spriteInfos.size())
            return m_spriteInfos[phase];
    }
    return firstSpriteInfo();
}

const TAppearanceInstanceSpriteInfoPtr& TAppearanceInstanceSpriteSequence::firstSpriteInfo() const
{
    static const TAppearanceInstanceSpriteInfoPtr none;
    return m_spriteInfos.empty() ? none : m_spriteInfos.front();
}

bool TAppearanceInstanceSpriteSequence::update(const qint64 nowMs)
{
    if (!m_animator)
        return false;
    const int before = m_animator->phase();
    m_animator->update(nowMs);
    return m_animator->phase() != before;
}

void TAppearanceInstanceSpriteSequence::setAnimatorKey(const qint64 keyMs, const qint64 nowMs)
{
    if (!m_animator)
        return;
    if (auto* moving = dynamic_cast<animation::TMovingOutfitAnimator*>(m_animator.get())) {
        // The animator stays alive while it takes the key.
        const std::shared_ptr<animation::TAnimator> keep = m_animator;
        moving->setKey(keyMs, nowMs);
    }
}

std::vector<std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>>
TAppearanceInstanceSpriteSequence::phaseSprites(const TAppearanceTypeSpriteInfo& spritesInfo, const TAppearancePattern& pattern,
                                                const int layer)
{
    // A missing sprite: sprite 0 of no spritesheet, an empty cell, not opaque.
    static const auto invalid =
        std::make_shared<const sprites::TSpriteFromSpritesheetInfo>(0, QString::fromUtf8(""), QRect(), false);
    std::vector<std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>> sprites;
    // The frame group's sprites were placed in their spritesheets at load.
    const auto& resolved = spritesInfo.sprites();
    const uint32_t frame = spritesInfo.patternWidth() * spritesInfo.patternHeight() * spritesInfo.patternDepth() * spritesInfo.layers();
    if (frame == 0)
        return sprites;
    const size_t phases = resolved.size() / frame;
    sprites.reserve(phases);
    for (size_t phase = 0; phase < phases; ++phase) {
        const size_t index = (((spritesInfo.patternDepth() * phase + pattern.z) * spritesInfo.patternHeight() + pattern.y)
                                  * spritesInfo.patternWidth()
                              + pattern.x)
                * spritesInfo.layers()
            + layer;
        if (index < resolved.size())
            sprites.push_back(resolved[index]);
        else
            sprites.push_back(invalid);
    }
    return sprites;
}

QRect TAppearanceInstanceSpriteSequence::boundingBox(const TAppearanceTypeSpriteInfo& spritesInfo, const int x)
{
    const auto& boxes = spritesInfo.boundingBoxes();
    if (x >= 0 && static_cast<size_t>(x) < boxes.size())
        return boxes[static_cast<size_t>(x)];
    if (!spritesInfo.sprites().empty() && spritesInfo.spriteSize().isValid())
        return QRect(QPoint(0, 0), spritesInfo.spriteSize());
    return QRect(0, 0, 32, 32);
}

namespace {

/// The sprite of `layer` at `pattern` in `phase`; none
/// past the frame group's sprites.
std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> patternSprite(const TAppearanceTypeSpriteInfo& spritesInfo,
                                                                         const TAppearancePattern& pattern, const int layer,
                                                                         const size_t phase)
{
    const auto& resolved = spritesInfo.sprites();
    const size_t index = (((spritesInfo.patternDepth() * phase + pattern.z) * spritesInfo.patternHeight() + pattern.y)
                              * spritesInfo.patternWidth()
                          + pattern.x)
            * spritesInfo.layers()
        + layer;
    if (index >= resolved.size())
        return nullptr;
    return resolved[index];
}

/// The frame group's phases.
size_t phaseCount(const TAppearanceTypeSpriteInfo& spritesInfo)
{
    const uint32_t frame = spritesInfo.patternWidth() * spritesInfo.patternHeight() * spritesInfo.patternDepth() * spritesInfo.layers();
    return frame == 0 ? 0 : spritesInfo.sprites().size() / frame;
}

} // namespace

void TCustomizableOutfitAppearanceInstanceSpriteSequence::appendPatternSprite(
    std::vector<TAppearanceInstanceSpriteInfoPtr>& layers, const TAppearancePattern& pattern, const size_t phase,
    const QRect& boundingBox) const
{
    auto sprite = patternSprite(*m_spritesInfo, pattern, 0, phase);
    if (m_spritesInfo->layers() <= 1) {
        layers.push_back(std::make_shared<TSpritesheetAppearanceInstanceSpriteInfo>(std::move(sprite), boundingBox));
        return;
    }
    auto mask = patternSprite(*m_spritesInfo, pattern, 1, phase);
    const TOutfitColors colors(m_customization.head, m_customization.body, m_customization.legs, m_customization.feet);
    layers.push_back(
        std::make_shared<TMaskedAppearanceInstanceSpriteInfo>(std::move(sprite), std::move(mask), colors, boundingBox));
}

void TCustomizableOutfitAppearanceInstanceSpriteSequence::setPattern(const TAppearancePattern& pattern, const qint64 startMs,
                                                                     const std::optional<TReverseAddons> reverseAddons)
{
    m_spriteInfos.clear();
    if (m_spritesInfo) {
        const size_t phases = phaseCount(*m_spritesInfo);
        const QRect box = boundingBox(*m_spritesInfo, pattern.x);
        m_spriteInfos.reserve(phases);
        for (size_t phase = 0; phase < phases; ++phase) {
            std::vector<TAppearanceInstanceSpriteInfoPtr> layers;
            layers.reserve(3);
            appendPatternSprite(layers, pattern, phase, box);
            const auto addon = [&](const bool worn, const int y) {
                if (worn && static_cast<int>(m_spritesInfo->patternHeight()) > y)
                    appendPatternSprite(layers, TAppearancePattern{ pattern.x, y, pattern.z }, phase, box);
            };
            bool reversed = false;
            if (reverseAddons) {
                switch (pattern.x) {
                case 0:
                    reversed = reverseAddons->north;
                    break;
                case 1:
                    reversed = reverseAddons->east;
                    break;
                case 2:
                    reversed = reverseAddons->south;
                    break;
                case 3:
                    reversed = reverseAddons->west;
                    break;
                default:
                    break;
                }
            }
            if (reversed) {
                addon(m_customization.secondAddon, 2);
                addon(m_customization.firstAddon, 1);
            } else {
                addon(m_customization.firstAddon, 1);
                addon(m_customization.secondAddon, 2);
            }
            if (layers.size() == 1)
                m_spriteInfos.push_back(std::move(layers.front()));
            else
                m_spriteInfos.push_back(std::make_shared<TLayeredAppearanceInstanceSpriteInfo>(std::move(layers), box));
        }
    }
    restartAnimation(startMs);
}

}
