#include "effects/effectremap.h"

namespace tibia::effects {

namespace {

/// Set on the GUI thread at startup, read there by the protocol handlers.
EffectRemapFn g_remap = nullptr;
void* g_remapUser = nullptr;

}

void setEffectRemapHook(const EffectRemapFn fn, void* const user)
{
    g_remap = fn;
    g_remapUser = fn ? user : nullptr;
}

TEffectRemap remapEffect(const uint16_t effectId)
{
    TEffectRemap remap;
    remap.drawId = effectId;
    if (g_remap) {
        TEffectRemap hooked;
        if (g_remap(g_remapUser, effectId, hooked) && hooked.drawId != 0)
            remap = hooked;
    }
    return remap;
}

}
