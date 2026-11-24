#include "renderer/worldframeview.h"

#include "appearances/appearanceinstance.h"
#include <client/position.h>
#include <tibia/appearances/appearancetype.h>
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <algorithm>

namespace tibia::qmlcomponents {

uint32_t appearanceFlagsOf(const appearances::TAppearanceType* type)
{
    if (!type)
        return 0;
    uint32_t flags = QTCLIENT_APPEARANCE_PRESENT;
    const auto set = [&flags](const bool on, const uint32_t bit) {
        if (on)
            flags |= bit;
    };
    set(type->isItem(), QTCLIENT_APPEARANCE_OBJECT);
    set(type->isCreature(), QTCLIENT_APPEARANCE_OUTFIT);
    set(type->isGround(), QTCLIENT_APPEARANCE_BANK);
    set(type->isGroundBorder(), QTCLIENT_APPEARANCE_CLIP);
    set(type->isOnBottom(), QTCLIENT_APPEARANCE_BOTTOM);
    set(type->isOnTop(), QTCLIENT_APPEARANCE_TOP);
    set(type->isNotWalkable(), QTCLIENT_APPEARANCE_UNPASS);
    set(type->isNotMoveable(), QTCLIENT_APPEARANCE_UNMOVE);
    set(type->blockProjectile(), QTCLIENT_APPEARANCE_UNSIGHT);
    set(type->isNotPathable(), QTCLIENT_APPEARANCE_AVOID);
    set(type->isFullGround(), QTCLIENT_APPEARANCE_FULL_BANK);
    set(type->isDontHide(), QTCLIENT_APPEARANCE_DONT_HIDE);
    set(type->isTranslucent(), QTCLIENT_APPEARANCE_TRANSLUCENT);
    set(type->isHangable(), QTCLIENT_APPEARANCE_HANG);
    set(type->hasElevation(), QTCLIENT_APPEARANCE_ELEVATION);
    set(type->isSplash(), QTCLIENT_APPEARANCE_LIQUID_POOL);
    set(type->isLyingCorpse(), QTCLIENT_APPEARANCE_LYING_OBJECT);
    set(type->isCorpse(), QTCLIENT_APPEARANCE_CORPSE);
    set(type->isPlayerCorpse(), QTCLIENT_APPEARANCE_PLAYER_CORPSE);
    set(type->isSingleDimension(), QTCLIENT_APPEARANCE_SINGLE_FIELD);
    set(type->hasLight(), QTCLIENT_APPEARANCE_LIGHT);
    return flags;
}

int peelFieldStack(const worldmap::TWorldMapField& field, QtClientFieldStackEntry* out, const int max)
{
    const auto& stack = field.appearances();
    const int count = static_cast<int>(stack.size());
    if (!out)
        return count;
    for (int i = 0; i < count && i < max; ++i) {
        QtClientFieldStackEntry entry{};
        if (const appearances::TAppearanceInstancePtr& instance = stack[static_cast<size_t>(i)]) {
            const appearances::TAppearanceType* type = instance->thingType();
            entry.flags = appearanceFlagsOf(type);
            entry.creature_id = instance->creatureId();
            if (type) {
                entry.type_id = type->getId();
                entry.height = static_cast<uint8_t>(std::clamp(type->getHeight(), 0, 255));
                entry.elevation = static_cast<uint8_t>(std::clamp(type->getElevation(), 0, 255));
            }
        }
        out[i] = entry;
    }
    return count;
}

int worldFieldStack(const int x, const int y, const int z, int* drawable, QtClientFieldStackEntry* out, const int max)
{
    if (drawable)
        *drawable = 0;
    if (x < 0 || y < 0 || z < 0 || x > 0xffff || y > 0xffff || z > 0xff)
        return -1;
    worldmap::TWorldMapField* field = worldmap::fieldAt(
        Position(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z)));
    if (!field)
        return -1;
    if (drawable)
        *drawable = field->isDrawable() ? 1 : 0;
    return peelFieldStack(*field, out, max);
}

}
