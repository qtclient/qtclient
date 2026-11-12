#include "magiceffectopacity.h"

#include <algorithm>
#include <array>

namespace tibia {
namespace client {
namespace {

std::array<float, ME_SOURCE_LAST + 1> makeDefaultOpacities()
{
    std::array<float, ME_SOURCE_LAST + 1> opacities{};
    opacities.fill(1.f);
    return opacities;
}

std::array<float, ME_SOURCE_LAST + 1>& opacities()
{
    static std::array<float, ME_SOURCE_LAST + 1> table = makeDefaultOpacities();
    return table;
}

} // namespace

float magicEffectOpacity(const MagicEffectSources source)
{
    if (static_cast<uint8_t>(source) > ME_SOURCE_LAST)
        return 1.f;
    return opacities()[source];
}

void setMagicEffectOpacity(const MagicEffectSources source, const float opacity)
{
    if (static_cast<uint8_t>(source) > ME_SOURCE_LAST)
        return;
    opacities()[source] = std::clamp(opacity, 0.f, 1.f);
}

void setMagicEffectOpacities(const float own, const float otherPlayer, const float monster, const float boss)
{
    setMagicEffectOpacity(ME_SOURCE_OWN, own);
    setMagicEffectOpacity(ME_SOURCE_OTHER_PLAYER, otherPlayer);
    setMagicEffectOpacity(ME_SOURCE_MONSTER, monster);
    setMagicEffectOpacity(ME_SOURCE_BOSS, boss);
}

} // namespace client
} // namespace tibia
