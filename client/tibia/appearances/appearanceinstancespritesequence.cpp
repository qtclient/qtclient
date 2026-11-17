#include "appearances/appearanceinstancespritesequence.h"

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

void TAppearanceInstanceSpriteSequence::setPattern(const TAppearancePattern& pattern, const qint64 startMs)
{
    m_spriteInfos.clear();
    if (m_spritesInfo) {
        const QRect box = boundingBox(*m_spritesInfo, pattern.x);
        for (auto& sprite : phaseSprites(*m_spritesInfo, pattern, 0))
            m_spriteInfos.push_back(std::make_shared<TSpritesheetAppearanceInstanceSpriteInfo>(std::move(sprite), box));
    }
    restartAnimation(startMs);
}

std::vector<std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>> TAppearanceInstanceSpriteSequence::spriteInfos() const
{
    return m_spriteInfos;
}

const std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>& TAppearanceInstanceSpriteSequence::currentSpriteInfo() const
{
    if (m_animator) {
        const auto phase = static_cast<size_t>(m_animator->phase());
        if (phase < m_spriteInfos.size())
            return m_spriteInfos[phase];
    }
    return firstSpriteInfo();
}

const std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo>& TAppearanceInstanceSpriteSequence::firstSpriteInfo() const
{
    static const std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo> none;
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

std::vector<std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>>
TAppearanceInstanceSpriteSequence::phaseSprites(const TAppearanceTypeSpriteInfo& spritesInfo, const TAppearancePattern& pattern,
                                                const int layer)
{
    static const auto invalid = std::make_shared<const sprites::TSpriteFromSpritesheetInfo>();
    std::vector<std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>> sprites;
    const auto& ids = spritesInfo.spriteIds();
    const uint32_t frame = spritesInfo.patternWidth() * spritesInfo.patternHeight() * spritesInfo.patternDepth() * spritesInfo.layers();
    if (frame == 0)
        return sprites;
    const size_t phases = ids.size() / frame;
    sprites.reserve(phases);
    for (size_t phase = 0; phase < phases; ++phase) {
        const size_t index = (((spritesInfo.patternDepth() * phase + pattern.z) * spritesInfo.patternHeight() + pattern.y)
                                  * spritesInfo.patternWidth()
                              + pattern.x)
                * spritesInfo.layers()
            + layer;
        if (index < ids.size())
            sprites.push_back(std::make_shared<const sprites::TSpriteFromSpritesheetInfo>(
                sprites::TSpriteSystem::instance().spriteInfo(ids[index], spritesInfo.isOpaque())));
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
    if (!spritesInfo.spriteIds().empty() && spritesInfo.spriteSize().isValid())
        return QRect(QPoint(0, 0), spritesInfo.spriteSize());
    return QRect(0, 0, 32, 32);
}

}
