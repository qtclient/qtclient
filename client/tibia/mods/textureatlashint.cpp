#include "mods/textureatlashint.h"

namespace tibia::mods {
namespace {

QtClientTextureAtlasHintFn g_hint = nullptr;

}

void setTextureAtlasHint(const QtClientTextureAtlasHintFn fn)
{
    g_hint = fn;
}

void applyTextureAtlasHint(int& defaultSide, int& sizeLimitCap)
{
    if (!g_hint)
        return;
    int side = defaultSide;
    int cap = sizeLimitCap;
    g_hint(&side, &cap);
    if (side >= 512)
        defaultSide = side;
    if (cap > 0)
        sizeLimitCap = cap;
}

}
