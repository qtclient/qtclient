#include "appearanceshape.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

namespace tibia::idleot {

namespace {

bool castsShadowByFlags(const tibia::appearances::TAppearanceType& type)
{
    if (type.isCreature())
        return true;
    if (!type.isItem() || type.isGround() || type.isGroundBorder() || type.isSplash() || type.isLyingCorpse() || type.isHangable())
        return false;
    if (type.isOnBottom() && type.hasElevation() && !type.isNotWalkable())
        return false;
    if (type.isNotWalkable() && type.isNotMoveable() && !type.blockProjectile() && !type.isOnBottom() && type.isSingleDimension()
        && !type.hasElevation())
        return false;
    return type.isNotWalkable() || type.blockProjectile() || !type.isSingleDimension() || type.hasElevation();
}

}

ThingShape thingShapeOf(const tibia::appearances::TAppearanceType& type)
{
    if (!type.isItem())
        return ThingShape::Auto;
    return tibia::appearances::TAppearancesManager::instance().thingShape(type.getId());
}

bool castsShadow(const tibia::appearances::TAppearanceType& type)
{
    const ThingShape shape = thingShapeOf(type);
    if (shape == ThingShape::Flat)
        return false;
    if (shape != ThingShape::Auto)
        return true;
    return castsShadowByFlags(type);
}

}
