#include "worldmappicker.h"

#include "creatures/creaturestorage.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapstorage.h"

#include <tibia/assets/thingtype.h>
#include <client/declarations.h>

#include <QRect>

namespace tibia::worldmap {

namespace {

using Appearances = std::vector<appearances::TAppearanceInstancePtr>;

ThingType* typeOf(const appearances::TAppearanceInstancePtr& appearance)
{
    return appearance ? appearance->thingType() : nullptr;
}

bool isCreatureAppearance(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->getCategory() == ThingCategoryCreature;
}

bool isBank(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isGround();
}
bool isClip(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isGroundBorder();
}
bool isBottom(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isOnBottom();
}
bool isTop(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isOnTop();
}
bool isLiquidPool(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isSplash();
}
bool isForceUse(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isForceUse();
}
bool isIgnoreLook(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isIgnoreLook();
}
bool isUnmove(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isNotMoveable();
}
bool isUnpass(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isNotWalkable();
}
bool isLootableCorpse(const appearances::TAppearanceInstancePtr& appearance)
{
    const ThingType* type = typeOf(appearance);
    return type && type->isCorpse() && type->isContainer();
}
bool isBankClipOrBottom(const appearances::TAppearanceInstancePtr& appearance)
{
    return isBank(appearance) || isClip(appearance) || isBottom(appearance);
}

/// Draw order top-down: everything after the leading ground, clip and bottom
/// objects in stack order, then those leading objects in reverse.
std::vector<int> topDownOrder(const Appearances& appearances)
{
    const int n = static_cast<int>(appearances.size());
    int leading = 0;
    while (leading < n && isBankClipOrBottom(appearances[leading]))
        ++leading;
    std::vector<int> order;
    order.reserve(n);
    for (int i = leading; i < n; ++i)
        order.push_back(i);
    for (int i = leading - 1; i >= 0; --i)
        order.push_back(i);
    return order;
}

const Appearances& appearancesOf(const TWorldMapField* field)
{
    static const Appearances empty;
    return field ? field->appearances() : empty;
}

TFieldObject fieldObjectAt(const TWorldMapField* field, const int stackPos)
{
    TFieldObject object;
    if (!field || stackPos < 0 || stackPos >= field->thingCount())
        return object;
    object.stackPosition = stackPos;
    object.appearance = field->appearanceAt(stackPos);
    object.creatureId = field->stackCreatureId(stackPos);
    return object;
}

}

uint16_t TFieldObject::clientId() const
{
    if (appearance)
        return appearance->id();
    if (creatureId) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId))
                if (row->outfitAppearance())
                    return row->outfitAppearance()->id();
        }
    }
    return 0;
}

namespace field {

TFieldObject useObject(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    const int n = static_cast<int>(appearances.size());
    if (n == 0)
        return {};
    if (n == 1)
        return fieldObjectAt(field, 0);
    int i = 0;
    for (; i < n - 1; ++i) {
        const auto& appearance = appearances[i];
        if (isForceUse(appearance))
            return fieldObjectAt(field, i);
        if (!isBankClipOrBottom(appearance) && !isTop(appearance) && !isCreatureAppearance(appearance))
            break;
    }
    for (int j = i; j >= 1; --j) {
        if (!isCreatureAppearance(appearances[j]) && !isLiquidPool(appearances[j]))
            return fieldObjectAt(field, j);
    }
    return fieldObjectAt(field, 0);
}

TFieldObject lookObject(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    const int n = static_cast<int>(appearances.size());
    if (n == 0)
        return {};
    TFieldObject candidate = fieldObjectAt(field, 0);
    for (int i = 0; i < n; ++i) {
        const auto& appearance = appearances[i];
        if (isIgnoreLook(appearance))
            continue;
        candidate = fieldObjectAt(field, i);
        if (isBankClipOrBottom(appearance) || (isTop(appearance) && i + 1 < n))
            continue;
        break;
    }
    return candidate;
}

TFieldObject moveObject(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    TFieldObject creature;
    for (const int i : topDownOrder(appearances)) {
        const auto& appearance = appearances[i];
        if (isTop(appearance))
            continue;
        if (isBankClipOrBottom(appearance))
            break;
        if (isCreatureAppearance(appearance)) {
            if (!creature)
                creature = fieldObjectAt(field, i);
            continue;
        }
        if (!isUnmove(appearance))
            return fieldObjectAt(field, i);
        break;
    }
    return creature;
}

TFieldObject multiUseObject(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    const int n = static_cast<int>(appearances.size());
    if (n == 0)
        return {};
    int i = 0;
    while (i < n - 1 && !isForceUse(appearances[i])
           && (isBankClipOrBottom(appearances[i]) || isTop(appearances[i])))
        ++i;
    if (isLiquidPool(appearances[i]) && i > 0)
        --i;
    return fieldObjectAt(field, i);
}

TFieldObject quickLootObject(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    for (const int i : topDownOrder(appearances)) {
        if (isLootableCorpse(appearances[i]))
            return fieldObjectAt(field, i);
    }
    return {};
}

TFieldObject creature(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        if (isCreatureAppearance(appearances[i]))
            return fieldObjectAt(field, i);
    }
    return {};
}

bool isPickable(const TWorldMapField* field)
{
    for (const auto& appearance : appearancesOf(field)) {
        if ((isBank(appearance) || isBottom(appearance)) && !isIgnoreLook(appearance))
            return true;
    }
    return false;
}

int walkability(const TWorldMapField* field)
{
    const Appearances& appearances = appearancesOf(field);
    if (appearances.empty())
        return 2;
    bool hasBank = false;
    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const auto& appearance = appearances[i];
        if (isCreatureAppearance(appearance)) {
            const uint32_t id = field->stackCreatureId(i);
            if (id == 0)
                return 2;
            tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
            if (!storage)
                return 2;
            const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(id);
            if (!row)
                return 2;
            if (!row->isPassable())
                return row->isPlayer() ? 2 : 1;
            return hasBank ? 0 : 2;
        }
        hasBank |= isBank(appearance);
        if (isUnpass(appearance))
            return 1;
        if (const ThingType* type = typeOf(appearance); type && type->hasNoMovementAnimation())
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
    TWorldMapStorage* storage = TWorldMapStorage::instance();
    if (!storage)
        return result;

    for (int z = provider.firstFloor(); z <= provider.lastFloor(); ++z) {
        const Position position = provider.fieldAt(mapPixel, z);
        if (!position.isValid())
            continue;
        TWorldMapField* field = storage->fieldAt(position);
        if (!field || field->isEmpty() || !field::isPickable(field))
            continue;
        result.valid = true;
        result.position = position;
        result.field = field;
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

uint32_t TWorldMapPicker::creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel)
{
    using Provider = qmlcomponents::TWorldMapRenderProvider;
    if (!provider.frame().valid)
        return 0;
    TWorldMapStorage* storage = TWorldMapStorage::instance();
    if (!storage)
        return 0;
    tibia::creatures::TCreatureStorage* creatureStorage = tibia::creatures::TCreatureStorage::instance();
    const Position origin = provider.firstField(provider.camera().z);
    for (int dx = 0; dx < provider.drawWidth(); ++dx) {
        for (int dy = 0; dy < provider.drawHeight(); ++dy) {
            const Position position = origin.translated(dx, dy);
            TWorldMapField* field = storage->fieldAt(position);
            if (!field || field->isEmpty())
                continue;
            const TFieldObject found = field::creature(field);
            if (!found)
                continue;
            const uint32_t creatureId = found.creatureId ? found.creatureId : field->stackCreatureId(found.stackPosition);
            if (creatureId == 0)
                continue;
            if (!creatureStorage)
                continue;
            const std::shared_ptr<tibia::creatures::TCreature> row = creatureStorage->creatureAt(creatureId);
            if (!row)
                continue;
            const Point walk = row->walkOffset();
            const QPoint pixel = provider.mapPixelOf(position) + QPoint(walk.x, walk.y);
            if (QRect(pixel, QSize(Provider::FieldSize, Provider::FieldSize)).contains(mapPixel))
                return creatureId;
        }
    }
    return 0;
}

}
