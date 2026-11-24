#include "creatures/creaturedecoration.h"

#include "animation/phaseanimator.h"
#include "client/const.h"
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace tibia::creatures {

namespace {

/// Set on the GUI thread at startup, read there while the map frame is built.
CreatureDecorationFn g_decorations = nullptr;
void* g_decorationsUser = nullptr;

/// The decorations of one call; more are dropped.
constexpr size_t MaxDecorations = 16;

/// The phase of `type`'s own animation at `nowMs`, every decoration of a type
/// in step (its idle animation started at time 0).
int animatedPhase(TAppearanceType* type, const qint64 nowMs)
{
    static animation::TIdleAnimators animators;
    const animation::TAnimationInfo* info = type->idleAnimation();
    if (!info && type->isAnimateAlways())
        info = type->movingAnimation();
    if (info)
        return animators.phaseAt(type, info, nowMs);

    const int phases = type->getAnimationPhases();
    if (phases <= 1)
        return 0;
    // An effect without durations: 75 ms per phase.
    if (type->isEffect()) {
        constexpr qint64 ticksPerPhase = 75;
        return static_cast<int>((nowMs % (ticksPerPhase * phases)) / ticksPerPhase);
    }
    // A creature that always animates: all its phases in a second.
    if (type->isAnimateAlways()) {
        const qint64 ticksPerPhase = std::max<qint64>(1, std::lround(1000.0 / phases));
        return static_cast<int>((nowMs % (ticksPerPhase * phases)) / ticksPerPhase);
    }
    return 0;
}

}

void setCreatureDecorationHook(const CreatureDecorationFn fn, void* const user)
{
    g_decorations = fn;
    g_decorationsUser = fn ? user : nullptr;
}

bool hasCreatureDecorationHook()
{
    return g_decorations != nullptr;
}

void creatureDecorations(const uint32_t creatureId, const bool overOutfit, const qint64 nowMs,
                         std::vector<TCreatureDecorationSprite>& out)
{
    if (!g_decorations)
        return;
    std::array<TCreatureDecoration, MaxDecorations> decorations{};
    const size_t count = std::min(g_decorations(g_decorationsUser, creatureId, overOutfit, decorations.data(),
                                                decorations.size()),
                                  decorations.size());
    const auto& appearances = appearances::TAppearancesManager::instance();
    for (size_t i = 0; i < count; ++i) {
        const TCreatureDecoration& decoration = decorations[i];
        const auto category = static_cast<ThingCategory>(decoration.category);
        if (category >= ThingLastCategory || !appearances.isValidDatId(decoration.appearanceId, category))
            continue;
        TAppearanceType* type = appearances.findAppearanceType(category, decoration.appearanceId);
        if (!type || type->isNull() || type->getAnimationPhases() == 0)
            continue;
        TCreatureDecorationSprite sprite;
        sprite.type = type;
        sprite.offset = QPoint(decoration.offsetX, decoration.offsetY);
        sprite.phase = decoration.phase < 0 ? animatedPhase(type, nowMs)
                                            : std::min(decoration.phase, type->getAnimationPhases() - 1);
        sprite.patternX = std::clamp(decoration.patternX, 0, std::max(1, type->getNumPatternX()) - 1);
        sprite.opacity = std::clamp(decoration.opacity, 0.f, 1.f);
        sprite.tint = decoration.tint;
        out.push_back(sprite);
    }
}

}
