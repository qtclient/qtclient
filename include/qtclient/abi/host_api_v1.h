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
#include "qtclient/abi/sound_api.h"
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

    void (*set_apply_image_tint_hook)(QtClientApplyImageTintFn fn);

    /** Opaque tip MissionBackend* / AttentionBackend* / InstanceBackend*. */
    void (*set_mission_backend)(void* backend);
    void (*set_attention_backend)(void* backend);
    void (*set_instance_backend)(void* backend);
    void (*set_login_sync_enqueue_hook)(QtClientLoginSyncEnqueueFn fn);

    void (*set_world_lighting_driver)(const QtClientLightingDriver* driver);
    void (*add_world_overlay_factory)(const QtClientWorldOverlayFactory* factory);
    void (*set_world_frame_hooks)(QtClientWorldFrameHooks hooks);

    void (*set_hd_scaler)(int factor, QtClientHdScaleFn fn, void* user);

    QtClientHostOsFn host_os;

    /** Nested domain tables (v2+). Null on older hosts. Session peel is additive (Phase 2+); field count grows at tail. */
    const QtClientSessionApiV1* session;
    const QtClientNotifyApiV1* notify;
    const QtClientOptionsApiV1* options;

    /** C wiring surface mirroring PackHostWire (v2+). Null on older hosts. */
    const QtClientPackHostApiV1* pack_host;

    /** Host API v6+ (read only when api_version >= 6): `fn` is asked about a map walk target
     *  (minimap / Cyclopedia map: no entering an unwalkable goal) the planner refuses, with the
     *  QTCLIENT_WALK_UNREACHABLE_* reason; nonzero takes it over (no failure message). A null
     *  `fn` leaves CipSoft's failure message. */
    void (*set_walk_unreachable_hook)(QtClientWalkUnreachableFn fn, void* user);

    /** Host API v8+ (read only when api_version >= 8): `fn` may draw another effect for a
     *  server magic effect id (QtClientEffectRemapFn). Null restores CipSoft's drawing. */
    void (*set_effect_remap_hook)(QtClientEffectRemapFn fn, void* user);
    /** Host API v8+: `fn` adds sprites under / over every drawn creature
     *  (QtClientCreatureDecorationFn). Null draws CipSoft's creatures only. */
    void (*set_creature_decoration_hook)(QtClientCreatureDecorationFn fn, void* user);

    /** Host API v8+: the current session's map field at (x, y, z): its stack (CipSoft order)
     *  into `out` (at most `max`), returns the stack's size; -1 when the session holds no such
     *  field. `drawable` (may be null) is set to whether the field draws anything. */
    int (*world_field_stack)(int x, int y, int z, int* drawable, QtClientFieldStackEntry* out, int max);
    /** Host API v8+: `fn` may raise the scene graph texture atlas (QtClientTextureAtlasHintFn);
     *  set it from qtclient_mod_register (the host asks before any window). Null: CipSoft's. */
    void (*set_texture_atlas_hint)(QtClientTextureAtlasHintFn fn);
    /** Host API v8+: the ambience hold (QtClientSoundApiV1). */
    const QtClientSoundApiV1* sound;

    /** Host API v8+: a Display Shortcuts entry (Options -> Display Shortcuts and the bar
     *  grid). `id` is stored in controlButtonsOptions; `label` is the options list
     *  text and tooltip; `image_utf8` is a `/images/...` or qrc path (null/empty: the
     *  label shows as text). `default_enabled`: appended to enabledButtons when the
     *  user has never seen the id. Calling again with the same id replaces it. */
    void (*register_button_bar_shortcut)(const char* id_utf8, const char* label_utf8, const char* image_utf8,
                                         int default_enabled, void (*run)(void* user), void* user);
    /** Host API v8+: an action a hotkey set can bind by `name_utf8` (the hotkey options
     *  page lists it with `description_utf8`, "Category: text"). `default_keys_utf8`
     *  (QKeySequence portable text, may be null/empty) runs it while it is unbound.
     *  Key repeats are ignored. Same name again replaces it. */
    void (*register_hotkey_action)(const char* name_utf8, const char* default_keys_utf8,
                                   const char* description_utf8, void (*run)(void* user), void* user);
} QtClientHostApiV1;

enum {
    /**
     * v2: nested session/notify/options; pack_host C wiring (additive tail).
     * v3: add_world_overlay_factory takes QtClientWorldOverlayFactory* (C vtable), not opaque C++.
     * v4: pack_host->game_action_router (additive tail of QtClientPackHostApiV1).
     * v5: session->container_storage .. character_configuration_loaded (current game session
     *     objects) and notify->subscribe_session_changed (additive tails); session_end fires when
     *     the current session disconnects. pack_host getters of per-session objects (chat_storage,
     *     imbuing_storage, store_messages) resolve at call time: re-read them on session change.
     * v6: set_walk_unreachable_hook (additive tail).
     * v7: session->team_finder_params / team_finder_members / team_finder_teams and
     *     notify->subscribe_team_finder_changed (additive tails).
     * v8 (breaking layout; MODULE packs are matched by host_build_id anyway):
     *     added set_effect_remap_hook, set_creature_decoration_hook, world_field_stack,
     *     set_texture_atlas_hint, sound (ambience hold), register_button_bar_shortcut,
     *     register_hotkey_action; session->set_creature_hidden and the map data peels
     *     (stored_fields_on_floor, is_aware_of_position, waypoint_costs_*, static_map_*);
     *     pack_host->map_overlay_hover / _click / _start_drag.
     *     set_world_frame_hooks takes after_build / filter_light_map (QtClientWorldFrameView)
     *     instead of request_outdoor_gather; the lighting driver gets begin_field /
     *     replaces_classic_light, end_frame without geometry, and CipSoft type data (type_id /
     *     type_flags / stack_index) instead of shape / casts_shadow / caster.
     *     pack_host: bind_quest_tracker / bind_quest_log take only the backend;
     *     add_product_options_page renamed add_options_page.
     *     Removed: set_ensure_mods_hook, set_effect_tint_hook, set_effect_variant_tint_hook,
     *     set_weather_driver; session: is_scene_frozen / set_scene_frozen, information
     *     channel, soft dead, effect variants / attached effects, set_item_thing_shape,
     *     can_travel_path(_async), set_path_avoid / clear_path_avoids (the travel path
     *     probe lives in the pack).
     */
    QTCLIENT_HOST_API_V1_VERSION = 8,
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
