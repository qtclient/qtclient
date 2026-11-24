#pragma once

/// C ABI for tip pack-host wiring (mirrors the PackHostWire ops absorb packs use).

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Host passes mission / attention / instance as opaque object pointers. */
typedef struct QtClientPackHostApiV1 {
    /** Returns non-zero when pack wiring is available (bind_quest_tracker set). */
    int (*valid)(void* user);

    /** v8: the extra quest rows' backend (tip mods::MissionBackend*); it handles its own actions. */
    void (*bind_quest_tracker)(void* user, void* mission_backend);
    void (*bind_quest_log)(void* user, void* mission_backend);
    void (*bind_instance_to_info_dialogs)(void* user, void* instance_backend);
    void (*bind_attention_to_button_bar)(void* user, void* attention_backend);
    /** v8 (was add_product_options_page): a pack's page (tip config::TOptionsPage*) in the options dialog. */
    void (*add_options_page)(void* user, int category, void* options_page);
    void (*open_options_page)(void* user, int page);
    void (*open_store)(void* user);
    void (*request_logout_via_button_bar)(void* user);
    void (*set_social_button_on_button_bar)(void* user, void* controller);
    void (*set_active_chat_item)(void* user, void* qquick_item);
    void (*install_create_character_controller)(void* user, void* controller);

    void* (*blessings_dialog)(void* user);
    void* (*npc_dialog)(void* user);
    void* (*chat_controller)(void* user);
    void* (*chat_storage)(void* user);
    void* (*store_messages)(void* user);
    void* (*store_image_manager)(void* user);
    void* (*minimap_controller)(void* user);
    void* (*sidebar_manager)(void* user);
    void* (*sidebar_widgets)(void* user);
    void* (*containers)(void* user);
    void* (*button_bar)(void* user);
    void* (*imbuing_dialog)(void* user);
    void* (*imbuing_storage)(void* user);
    void* (*session_bootstrap)(void* user);

    void* (*minimap_panel_item)(void* user);
    void* (*inventory_panel_item)(void* user);
    void* (*button_bar_panel_item)(void* user);

    /** game::TInternalGameActionRouter* (<qtclient/game/gameactionrouter.h>) the pack's
     *  publishGameAction signals connect to; null before the game window exists.
     *  Host API v4+: read it only when api_version >= 4 (older tables end before it). */
    void* (*game_action_router)(void* user);

    /** Map window overlay input, API v8+: the mouse over chrome drawn on the map
     *  window (hovering 0: it left), a click, or a drag start at a map point.
     *  Coordinates are map window pixels. */
    void (*map_overlay_hover)(void* user, int hovering, double x, double y);
    void (*map_overlay_click)(void* user, int x, int y, int mouse_button, int keyboard_modifiers);
    void (*map_overlay_start_drag)(void* user, int x, int y);
} QtClientPackHostApiV1;

#ifdef __cplusplus
}
#endif
