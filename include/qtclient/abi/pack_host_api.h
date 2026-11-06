#pragma once

/// C ABI for tip pack-host wiring (mirrors PackHostWire ops used by IdleOT suite).

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Host passes mission / attention / instance as opaque object pointers. */
typedef void (*QtClientBrowseFn)(void* browse_ctx, const char* filter_utf8);

typedef struct QtClientPackHostApiV1 {
    /** Returns non-zero when pack wiring is available (bind_quest_tracker set). */
    int (*valid)(void* user);

    void (*bind_quest_tracker)(void* user, void* mission_backend, QtClientBrowseFn browse, void* browse_ctx);
    void (*bind_quest_log)(void* user, void* mission_backend, QtClientBrowseFn browse, void* browse_ctx);
    void (*bind_instance_to_info_dialogs)(void* user, void* instance_backend);
    void (*bind_attention_to_button_bar)(void* user, void* attention_backend);
    void (*add_product_options_page)(void* user, int category, void* options_page);
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
} QtClientPackHostApiV1;

#ifdef __cplusplus
}
#endif
