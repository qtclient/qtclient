#pragma once

/// Deprecated C++ Mod API v1 (ABSORB_INTO_QTCLIENT / transitional).
///
/// New packs must use host-pushed C ABI: `#include <qtclient/abi/host_api_v1.h>`
/// and export `qtclient_mod_register(api, ctx)`. Do not call
/// `qtclient_mod_host_api_v1()` from a runtime MODULE (that reintroduces tip imports).

#include "mod_registry.h"
#include "weather.h"
#include "worldview.h"
#include "hdscale.h"

#include <cstdint>

namespace qtclient {

struct ModApiV1 {
    static constexpr int kVersion = 1;

    int apiVersion = kVersion;
    const char* hostBuildId = "";

    void (*registerMod)(tibia::mods::ModEntry entry) = nullptr;
    void (*registerEarlyBootstrap)(tibia::mods::ModBootstrapFn fn) = nullptr;
    void (*setEnsureModsHook)(tibia::mods::EnsureModsFn fn) = nullptr;

    void (*setEffectTintHook)(tibia::mods::EffectTintFn fn) = nullptr;
    void (*setEffectVariantTintHook)(tibia::mods::EffectVariantTintFn fn) = nullptr;
    void (*setApplyImageTintHook)(tibia::mods::ApplyImageTintFn fn) = nullptr;

    void (*setMissionBackend)(tibia::mods::MissionBackend* backend) = nullptr;
    void (*setAttentionBackend)(tibia::mods::AttentionBackend* backend) = nullptr;
    void (*setInstanceBackend)(tibia::mods::InstanceBackend* backend) = nullptr;
    void (*setLoginSyncEnqueueHook)(tibia::mods::LoginSyncEnqueueFn fn) = nullptr;
    void (*setFeatureExtrasHook)(tibia::mods::FeatureExtrasFn fn) = nullptr;

    void (*setWorldLightingDriver)(tibia::mods::WorldLightingDriver* driver) = nullptr;
    void (*addWorldOverlayFactory)(tibia::mods::WorldOverlayFactory* factory) = nullptr;
    void (*setWorldFrameHooks)(tibia::mods::WorldFrameHooks hooks) = nullptr;
    void (*setWeatherDriver)(tibia::mods::WeatherDriver* driver) = nullptr;

    void (*setHdScaler)(int factor, tibia::sprites::HdScaleFn fn) = nullptr;
};

} // namespace qtclient

#if defined(_WIN32)
#  define QTCLIENT_HOST_API_EXPORT extern "C" __declspec(dllexport)
#  define QTCLIENT_HOST_API_IMPORT extern "C" __declspec(dllimport)
#else
#  define QTCLIENT_HOST_API_EXPORT extern "C" __attribute__((visibility("default")))
#  define QTCLIENT_HOST_API_IMPORT extern "C"
#endif

#if defined(QTCLIENT_MOD_BUILD)
QTCLIENT_HOST_API_IMPORT const qtclient::ModApiV1* qtclient_mod_host_api_v1();
#else
QTCLIENT_HOST_API_EXPORT const qtclient::ModApiV1* qtclient_mod_host_api_v1();
#endif
