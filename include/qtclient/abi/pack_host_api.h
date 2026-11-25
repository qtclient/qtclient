#pragma once

/// Game window chrome a pack plugs into (host API v9: only Qt-typed and plain entries; the
/// tip-object getters of v2..v8 are gone, see host_api_v1.h). Host:
/// client/tibia/mods/pack_host_abi.cpp. Every function takes the table's `user` argument
/// (pass null) and runs on the GUI thread; each is a no-op / null before the game window
/// exists (`valid`).

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A pack's options page (QtClientPackHostApiV1.add_options_page). */
typedef struct QtClientOptionsPageV1 {
    size_t struct_size;
    /** The options dialog's category (CipSoft EDialogCategories) the page goes under. */
    int category;
    /** The page's QML controller (the page item binds it). Must outlive the options dialog
     *  (the application). */
    QtClientQObjectPtr page;
    void* user;
    /** The options dialog opens or resets: load the pending values (CipSoft TOptionsPage
     *  load). May be null. */
    void (*load)(void* user);
    /** Apply / Ok: store the pending values (CipSoft TOptionsPage apply). May be null. */
    void (*apply)(void* user);
} QtClientOptionsPageV1;

typedef struct QtClientPackHostApiV1 {
    /** Nonzero once the game window exists (the chrome entries below work). */
    int (*valid)(void* user);

    /** v9 (was add_options_page(void*, int, void* TOptionsPage*)): adds a page to the options
     *  dialog (CipSoft TOptionsMenuDialogController::addExtensionPage). Copied. */
    void (*add_options_page)(void* user, const QtClientOptionsPageV1* page);
    /** Opens the options dialog on `page` (CipSoft's page id). */
    void (*open_options_page)(void* user, int page);
    void (*open_store)(void* user);
    /** The button bar's logout (CipSoft's confirmation included). */
    void (*request_logout_via_button_bar)(void* user);
    /** The chat item CipSoft's chat controller drives (a pack's floating chat or CipSoft's
     *  docked one). */
    void (*set_active_chat_item)(void* user, QtClientQQuickItemPtr item);

    /** CipSoft's panels in the right sidebar's upper pane. */
    QtClientQQuickItemPtr (*minimap_panel_item)(void* user);
    QtClientQQuickItemPtr (*inventory_panel_item)(void* user);
    QtClientQQuickItemPtr (*button_bar_panel_item)(void* user);

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
