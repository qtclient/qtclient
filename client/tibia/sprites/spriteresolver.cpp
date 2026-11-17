#include "sprites/spriteresolver.h"

#include "sprites/singlesprite.h"
#include "sprites/spriteinspritesheet.h"
#include "sprites/spritesheetmanager.h"

namespace tibia::sprites {

TSpriteResolver::TSpriteResolver(std::shared_ptr<TSpritesheetManager> spritesheets, QObject* parent)
    : QObject(parent)
    , m_spritesheets(std::move(spritesheets))
{
}

std::shared_ptr<TSprite> TSpriteResolver::resolveSpritesheetSprite(const TSpriteFromSpritesheetInfo& info) const
{
    const TSpritesheetPointer spritesheet = m_spritesheets->getSpritesheet(info.spritesheetId);
    if (!spritesheet)
        return nullptr;
    const QRect area(QPoint(0, 0), QPoint(info.cell.right() - info.cell.left(), info.cell.bottom() - info.cell.top()));
    return std::make_shared<TSpriteInSpritesheet>(0.0, spritesheet, info.cell, area, info.opaque);
}

std::shared_ptr<TSprite> TSpriteResolver::resolveAssetSpritesheet(const TSpritesheetID& spritesheetId,
                                                                   const float scale) const
{
    const TSpritesheetPointer spritesheet = m_spritesheets->getSpritesheet(spritesheetId);
    if (!spritesheet)
        return nullptr;
    auto sprite = std::make_shared<TSingleSprite>(scale, *spritesheet);
    sprite->setCacheable(false);
    return sprite;
}

}
