#include "qtclient/mod_api_v1.h"

extern "C" const char qtclient_host_build_id[];

namespace {

const qtclient::ModApiV1 g_api = {
    .apiVersion = qtclient::ModApiV1::kVersion,
    .hostBuildId = qtclient_host_build_id,
    .registerMod = &tibia::mods::registerMod,
    .registerEarlyBootstrap = &tibia::mods::registerEarlyBootstrap,
    .setEnsureModsHook = &tibia::mods::setEnsureModsHook,
    .setEffectTintHook = &tibia::mods::setEffectTintHook,
    .setEffectVariantTintHook = &tibia::mods::setEffectVariantTintHook,
    .setApplyImageTintHook = &tibia::mods::setApplyImageTintHook,
    .setMissionBackend = &tibia::mods::setMissionBackend,
    .setAttentionBackend = &tibia::mods::setAttentionBackend,
    .setInstanceBackend = &tibia::mods::setInstanceBackend,
    .setLoginSyncEnqueueHook = &tibia::mods::setLoginSyncEnqueueHook,
    .setWorldLightingDriver = &tibia::mods::setWorldLightingDriver,
    .addWorldOverlayFactory = &tibia::mods::addWorldOverlayFactory,
    .setWorldFrameHooks = &tibia::mods::setWorldFrameHooks,
    .setWeatherDriver = &tibia::mods::setWeatherDriver,
    .setHdScaler = &tibia::sprites::setHdScaler,
};

} // namespace

QTCLIENT_HOST_API_EXPORT const qtclient::ModApiV1* qtclient_mod_host_api_v1()
{
    return &g_api;
}
