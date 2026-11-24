#pragma once

/// Host extension point: sprites an extension draws with a creature, under or
/// over its outfit. Without a hook the map draws CipSoft's creatures only.

#include "client/declarations.h"

#include <QPoint>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tibia::creatures {

/// One sprite an extension draws with a creature.
struct TCreatureDecoration {
    uint16_t appearanceId = 0;
    /// ThingCategory of `appearanceId`.
    uint8_t category = 0;
    /// Recolour key handed to the image tint hook when the sprite texture is made; 0 none.
    uint8_t tint = 0;
    /// Pixels from the creature's drawn field corner.
    int16_t offsetX = 0;
    int16_t offsetY = 0;
    /// The animation phase; below 0 the appearance's own animation on the frame clock.
    int32_t phase = -1;
    int32_t patternX = 0;
    float opacity = 1.f;
};

/// Fills up to `cap` decorations of creature `creatureId` on one side of its
/// outfit and returns how many it wrote. Called on the GUI thread while the map
/// frame is built, twice per drawn creature: keep it cheap.
using CreatureDecorationFn = size_t (*)(void* user, uint32_t creatureId, bool overOutfit, TCreatureDecoration* out,
                                        size_t cap);

void setCreatureDecorationHook(CreatureDecorationFn fn, void* user);
bool hasCreatureDecorationHook();

/// A decoration resolved for drawing.
struct TCreatureDecorationSprite {
    TAppearanceType* type = nullptr;
    QPoint offset;
    int phase = 0;
    int patternX = 0;
    float opacity = 1.f;
    uint8_t tint = 0;
};

/// Appends the decorations of `creatureId` on one side of its outfit at
/// `nowMs` (frame clock); unknown appearances are left out. Nothing without a hook.
void creatureDecorations(uint32_t creatureId, bool overOutfit, qint64 nowMs,
                         std::vector<TCreatureDecorationSprite>& out);

}
