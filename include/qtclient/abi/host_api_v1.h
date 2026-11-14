#pragma once

/// Host-pushed pack API v1 — the capability surface packs may use.
/// Extending the host means adding fields here (bump api_version when breaking).
/// Packs never look up tip symbols; the tip passes this table into qtclient_mod_register.

#include "qtclient/abi/lighting.h"
#include "qtclient/abi/notify_api.h"
#include "qtclient/abi/options_api.h"
#include "qtclient/abi/registry.h"
#include "qtclient/abi/session.h"
#include "qtclient/abi/session_api.h"
#include "qtclient/abi/pack_host_api.h"
#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QtClientHostApiV1 {
    int api_version;
    const char* host_build_id;

    void (*register_mod)(QtClientModEntry entry);
    void (*register_early_bootstrap)(QtClientModBootstrapFn fn);
    void (*set_ensure_mods_hook)(QtClientEnsureModsFn fn);

    void (*set_effect_tint_hook)(QtClientEffectTintFn fn);
    void (*set_effect_variant_tint_hook)(QtClientEffectVariantTintFn fn);
    void (*set_apply_image_tint_hook)(QtClientApplyImageTintFn fn);

    /** Opaque tip MissionBackend* / AttentionBackend* / InstanceBackend*. */
    void (*set_mission_backend)(void* backend);
    void (*set_attention_backend)(void* backend);
    void (*set_instance_backend)(void* backend);
    void (*set_login_sync_enqueue_hook)(QtClientLoginSyncEnqueueFn fn);

    void (*set_world_lighting_driver)(const QtClientLightingDriver* driver);
    void (*add_world_overlay_factory)(const QtClientWorldOverlayFactory* factory);
    void (*set_world_frame_hooks)(QtClientWorldFrameHooks hooks);
    void (*set_weather_driver)(void* driver);

    void (*set_hd_scaler)(int factor, QtClientHdScaleFn fn, void* user);

    QtClientHostOsFn host_os;
    QtClientEnableFeatureFn enable_feature;

    /** Nested domain tables (v2+). Null on older hosts. Session peel is additive (Phase 2+); field count grows at tail. */
    const QtClientSessionApiV1* session;
    const QtClientNotifyApiV1* notify;
    const QtClientOptionsApiV1* options;

    /** C wiring surface mirroring PackHostWire (v2+). Null on older hosts. */
    const QtClientPackHostApiV1* pack_host;
} QtClientHostApiV1;

enum {
    /**
     * v2: nested session/notify/options; pack_host C wiring (additive tail).
     * v3: add_world_overlay_factory takes QtClientWorldOverlayFactory* (C vtable), not opaque C++.
     */
    QTCLIENT_HOST_API_V1_VERSION = 3,
};

/**
 * Sole symbol a pack MODULE must export.
 * Tip dlopens the module and calls this with a filled API table.
 * Packs must not import tip symbols — use only `api`.
 */
typedef void (*QtClientModRegisterFn)(const QtClientHostApiV1* api, const QtClientModLoadCtx* ctx);

#define QTCLIENT_MOD_REGISTER_SYMBOL "qtclient_mod_register"

#if defined(_WIN32)
#  ifdef __cplusplus
#    define QTCLIENT_MOD_EXPORT extern "C" __declspec(dllexport)
#  else
#    define QTCLIENT_MOD_EXPORT __declspec(dllexport)
#  endif
#else
#  ifdef __cplusplus
#    define QTCLIENT_MOD_EXPORT extern "C" __attribute__((visibility("default")))
#  else
#    define QTCLIENT_MOD_EXPORT __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
}
#endif
