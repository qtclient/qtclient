#include "worldmappicker.h"

#include "renderer/worldmaprenderprovider.h"

#include <tibia/assets/thingtype.h>
#include <tibia/world/world.h>

#include <QRect>

namespace tibia::worldmap {

namespace {

using Things = std::vector<ThingPtr>;

ThingType* itemType(const ThingPtr& thing)
{
    return thing && thing->isItem() ? thing->getThingType() : nullptr;
}

bool isBank(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isGround(); }
bool isClip(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isGroundBorder(); }
bool isBottom(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isOnBottom(); }
bool isTop(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isOnTop(); }
bool isLiquidPool(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isSplash(); }
bool isForceUse(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isForceUse(); }
bool isIgnoreLook(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isIgnoreLook(); }
bool isUnmove(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isNotMoveable(); }
bool isUnpass(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isNotWalkable(); }
bool isLootableCorpse(const ThingPtr& thing) { auto* t = itemType(thing); return t && t->isCorpse() && t->isContainer(); }
bool isBankClipOrBottom(const ThingPtr& thing) { return isBank(thing) || isClip(thing) || isBottom(thing); }

/// Draw order top-down: everything after the leading ground, clip and bottom
/// objects in stack order, then those leading objects in reverse.
std::vector<int> topDownOrder(const Things& things)
{
    const int n = static_cast<int>(things.size());
    int leading = 0;
    while (leading < n && isBankClipOrBottom(things[leading]))
        ++leading;
    std::vector<int> order;
    order.reserve(n);
    for (int i = leading; i < n; ++i)
        order.push_back(i);
    for (int i = leading - 1; i >= 0; --i)
        order.push_back(i);
    return order;
}

}

namespace field {

TFieldObject useObject(const TilePtr& field)
{
    const Things& things = field->getThings();
    const int n = static_cast<int>(things.size());
    if (n == 0)
        return {};
    if (n == 1)
        return { things[0], 0 };
    int i = 0;
    for (; i < n - 1; ++i) {
        const ThingPtr& thing = things[i];
        if (isForceUse(thing))
            return { thing, i };
        if (!isBankClipOrBottom(thing) && !isTop(thing) && !thing->isCreature())
            break;
    }
    for (int j = i; j >= 1; --j) {
        if (!things[j]->isCreature() && !isLiquidPool(things[j]))
            return { things[j], j };
    }
    return { things[0], 0 };
}

TFieldObject lookObject(const TilePtr& field)
{
    const Things& things = field->getThings();
    const int n = static_cast<int>(things.size());
    if (n == 0)
        return {};
    TFieldObject candidate{ things[0], 0 };
    for (int i = 0; i < n; ++i) {
        const ThingPtr& thing = things[i];
        if (isIgnoreLook(thing))
            continue;
        candidate = { thing, i };
        if (isBankClipOrBottom(thing) || (isTop(thing) && i + 1 < n))
            continue;
        break;
    }
    return candidate;
}

TFieldObject moveObject(const TilePtr& field)
{
    const Things& things = field->getThings();
    TFieldObject creature;
    for (const int i : topDownOrder(things)) {
        const ThingPtr& thing = things[i];
        if (isTop(thing))
            continue;
        if (isBankClipOrBottom(thing))
            break;
        if (thing->isCreature()) {
            if (!creature)
                creature = { thing, i };
            continue;
        }
        if (!isUnmove(thing))
            return { thing, i };
        break;
    }
    return creature;
}

TFieldObject multiUseObject(const TilePtr& field)
{
    const Things& things = field->getThings();
    const int n = static_cast<int>(things.size());
    if (n == 0)
        return {};
    int i = 0;
    while (i < n - 1 && !isForceUse(things[i]) && (isBankClipOrBottom(things[i]) || isTop(things[i])))
        ++i;
    if (isLiquidPool(things[i]) && i > 0)
        --i;
    return { things[i], i };
}

TFieldObject quickLootObject(const TilePtr& field)
{
    const Things& things = field->getThings();
    for (const int i : topDownOrder(things)) {
        if (isLootableCorpse(things[i]))
            return { things[i], i };
    }
    return {};
}

TFieldObject creature(const TilePtr& field)
{
    const Things& things = field->getThings();
    for (int i = 0; i < static_cast<int>(things.size()); ++i) {
        if (things[i]->isCreature())
            return { things[i], i };
    }
    return {};
}

bool isPickable(const TilePtr& field)
{
    for (const ThingPtr& thing : field->getThings()) {
        if ((isBank(thing) || isBottom(thing)) && !isIgnoreLook(thing))
            return true;
    }
    return false;
}

int walkability(const TilePtr& field)
{
    const Things& things = field->getThings();
    if (things.empty())
        return 2;
    bool hasBank = false;
    for (const ThingPtr& thing : things) {
        if (thing->isCreature()) {
            const auto creature = thing->static_self_cast<Creature>();
            if (!creature->isPassable())
                return creature->isPlayer() ? 2 : 1;
            return hasBank ? 0 : 2;
        }
        hasBank |= isBank(thing);
        if (isUnpass(thing))
            return 1;
        if (auto* type = itemType(thing); type && type->hasNoMovementAnimation())
            return 2;
    }
    return hasBank ? 0 : 2;
}

}

TWorldMapPick TWorldMapPicker::pick(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel)
{
    TWorldMapPick result;
    if (!provider.frame().valid)
        return result;

    for (int z = provider.firstFloor(); z <= provider.lastFloor(); ++z) {
        const Position position = provider.fieldAt(mapPixel, z);
        if (!position.isValid())
            continue;
        const TilePtr& tile = tibia::world::fields().getTile(position);
        if (!tile || !field::isPickable(tile))
            continue;
        result.valid = true;
        result.position = position;
        result.field = tile;
        break;
    }
    if (!result.valid)
        return result;

    result.playerFloorPosition = provider.fieldAt(mapPixel, provider.camera().z);
    result.lightness = provider.fieldLightness(result.playerFloorPosition);
    result.walkability = field::walkability(result.field);
    result.use = field::useObject(result.field);
    result.look = field::lookObject(result.field);
    result.move = field::moveObject(result.field);
    result.multiUse = field::multiUseObject(result.field);
    result.quickLoot = field::quickLootObject(result.field);
    result.creature = field::creature(result.field);
    return result;
}

CreaturePtr TWorldMapPicker::creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel)
{
    using Provider = qmlcomponents::TWorldMapRenderProvider;
    if (!provider.frame().valid)
        return nullptr;
    const Position origin = provider.firstField(provider.camera().z);
    for (int dx = 0; dx < provider.drawWidth(); ++dx) {
        for (int dy = 0; dy < provider.drawHeight(); ++dy) {
            const Position position = origin.translated(dx, dy);
            const TilePtr& tile = tibia::world::fields().getTile(position);
            if (!tile)
                continue;
            const TFieldObject found = field::creature(tile);
            if (!found)
                continue;
            const auto creature = found.object->static_self_cast<Creature>();
            const Point walk = tibia::world::motion().offset(*creature);
            const QPoint pixel = provider.mapPixelOf(position) + QPoint(walk.x, walk.y);
            if (QRect(pixel, QSize(Provider::FieldSize, Provider::FieldSize)).contains(mapPixel))
                return creature;
        }
    }
    return nullptr;
}

}
