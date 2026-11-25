#pragma once

/// Game window surfaces (host API v9): CipSoft's sidebars and sidebar widgets, the minimap
/// panel, the Cyclopedia map renderer item, the store image base and sprite pixels.
/// Replaces the tip's TSidebarManager / TSidebarWidgetsManager / TSidebarWidget /
/// TContainerStorageController / TMinimapController / TMinimapRendererQuickItem /
/// TWorldMapCamera / TDynamicallyLoadedImageManager / sprites::spriteImageDecoded for packs.
/// Host: client/tibia/mods/ui_abi.cpp.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------------------
 * Sidebars. Ids are CipSoft's: right sidebars 0 (next to the map) .. n-1, left sidebars
 * QTCLIENT_LEFT_SIDEBAR_ID_BASE (next to the map) .. base + n - 1.
 * Sidebar widgets are named by host-assigned widget ids (uint32_t, 0 = none), stable while
 * the widget exists. Widget kinds are CipSoft's EWidgetType values
 * (QTCLIENT_SIDEBAR_WIDGET_*); pack widgets use kinds >= QTCLIENT_SIDEBAR_WIDGET_EXTENSION_BASE.
 * ---------------------------------------------------------------------------------------- */

enum {
    QTCLIENT_SIDEBAR_SIDE_RIGHT = 0,
    QTCLIENT_SIDEBAR_SIDE_LEFT = 1,
    QTCLIENT_LEFT_SIDEBAR_ID_BASE = 100,
    /** widget_parent_sidebar: the widget sits in no sidebar's widget pane (an upper pane,
     *  a pack dock, or nowhere). CipSoft TSidebarWidget::NoSidebar. */
    QTCLIENT_SIDEBAR_NONE = 0xff,

    /** CipSoft TSidebarWidgetsManager::EWidgetType. */
    QTCLIENT_SIDEBAR_WIDGET_SKILLS = 0,
    QTCLIENT_SIDEBAR_WIDGET_BATTLE_LIST = 1,
    QTCLIENT_SIDEBAR_WIDGET_PARTY_LIST = 2,
    QTCLIENT_SIDEBAR_WIDGET_VIP = 3,
    QTCLIENT_SIDEBAR_WIDGET_UNJUSTIFIED_POINTS = 4,
    QTCLIENT_SIDEBAR_WIDGET_PREY = 5,
    QTCLIENT_SIDEBAR_WIDGET_SPELL_LIST = 6,
    QTCLIENT_SIDEBAR_WIDGET_CONTAINER = 8,
    QTCLIENT_SIDEBAR_WIDGET_QUEST_TRACKER = 10,
    QTCLIENT_SIDEBAR_WIDGET_PLAYER_GUIDE = 24, /* the last CipSoft kind */
    QTCLIENT_SIDEBAR_WIDGET_EXTENSION_BASE = 100,

    /** QtClientUiApiV1.subscribe_sidebars events (a, b per event). */
    QTCLIENT_SIDEBARS_COUNT_CHANGED = 1,          /* numberOfSidebarsChanged; a, b = 0 */
    QTCLIENT_SIDEBARS_CONFIGURATION_CHANGED = 2,  /* widgetConfigurationChanged (batched) */
    QTCLIENT_SIDEBARS_WIDGET_HIDDEN = 3,          /* a = kind */
    QTCLIENT_SIDEBARS_WIDGET_WANTS_TO_MOVE = 4,   /* a = widget id, b = target sidebar id */
    QTCLIENT_SIDEBARS_CONTAINER_WIDGETS_CHANGED = 5,
};

/** A container's sidebar widget (CipSoft TContainerStorageController::widgets). */
typedef struct QtClientContainerWidget {
    uint8_t container_id;
    uint32_t widget_id;
} QtClientContainerWidget;

/** A pack sidebar widget: a CipSoft TSidebarWidget (TSidebarWidgetController over CipSoft's
 *  TSidebarWidgetControllerBase) with the pack's QML item, registered with the widgets
 *  manager under `kind` (CipSoft WidgetHooks). The item's `widgetController` is the host's
 *  widget controller, as for CipSoft widgets. */
typedef struct QtClientSidebarWidgetDescV1 {
    size_t struct_size;
    /** >= QTCLIENT_SIDEBAR_WIDGET_EXTENSION_BASE; unique per kind. */
    int kind;
    /** Stable name of the kind (layout files, widget_kind_of_name), e.g. "minimap". Copied. */
    const char* name_utf8;
    /** The widget item's QML (as QtClientDialogDescV1.qml_url). Copied; made at once. */
    const char* qml_url;
    void* user;
    /** The manager opens the kind: return nonzero when the widget may open (the host then
     *  places it). May be null (always). */
    int (*open)(void* user);
    /** The widget left its sidebar (the manager closed it). May be null. */
    void (*closed)(void* user);
    /** The session changed: the manager drops the widget without saving. May be null. */
    void (*discarded)(void* user);
    /** The widget asked to close (CipSoft widgetClosed: its header close button, or CipSoft
     *  closing it for lack of room). May be null. */
    void (*close_requested)(void* user);
} QtClientSidebarWidgetDescV1;

/** What the minimap panel shows (CipSoft TMinimapVisibleArea). */
typedef struct QtClientMinimapView {
    /** The top-left field and size in fields, on floor z. */
    int x;
    int y;
    int z;
    int width;
    int height;
    /** Item pixels per field. */
    float scale;
    /** World field -> item pixel transform (QMatrix4x4::constData order: column-major). */
    float world_to_item[16];
} QtClientMinimapView;

/** An extra zone the Cyclopedia map renderer draws like a highlighted subarea (CipSoft
 *  TMinimapRendererQuickItem::TExtraZoneMask). */
typedef struct QtClientZoneMask {
    /** Zone id (areaClicked reports it). */
    int id;
    /** The mask image file (a pack path or URL; its pixels cover `x, y, width, height`). */
    const char* file_utf8;
    int x;
    int y;
    int width;
    int height;
    int z;
} QtClientZoneMask;

enum {
    /** map_renderer_subscribe events. */
    QTCLIENT_MAP_RENDERER_VIEWPORT_CHANGED = 1, /* a, b = 0 */
    QTCLIENT_MAP_RENDERER_AREA_CLICKED = 2,     /* a = area / subarea / zone id, b = is_area */

    /** map_renderer_set_tile_type (CipSoft EMinimapTileType). */
    QTCLIENT_MAP_TILE_SATELLITE = 0,
    QTCLIENT_MAP_TILE_MINIMAP = 1,
    QTCLIENT_MAP_TILE_LOCAL_MINIMAP = 2,
};

typedef struct QtClientUiApiV1 {
    size_t struct_size;

    /* --- sidebars (CipSoft TSidebarManager). Null / 0 before the game window exists. --- */

    int (*sidebar_count)(int side);
    void (*set_sidebar_count)(int side, int count);
    /** The open sidebars' ids (list). */
    size_t (*sidebar_ids)(uint32_t* out, size_t cap);
    int (*has_sidebar)(uint32_t sidebar_id);
    /** The sidebar's item, its upper pane (equipment / status / options panels) and its lower
     *  (widget) pane. */
    QtClientQQuickItemPtr (*sidebar_item)(uint32_t sidebar_id);
    QtClientQQuickItemPtr (*sidebar_upper_pane)(uint32_t sidebar_id);
    QtClientQQuickItemPtr (*sidebar_lower_pane)(uint32_t sidebar_id);
    /** The free height of the sidebar's widget pane, in pixels. */
    int (*sidebar_remaining_height)(uint32_t sidebar_id);
    /** A fresh row identifier for a panel in the sidebar's upper pane (CipSoft
     *  reserveIdentifier). */
    int (*sidebar_reserve_identifier)(uint32_t sidebar_id);
    /** Puts the widget into a sidebar the manager picks (CipSoft addWidget), or into
     *  `sidebar_id` at `index` of its widget pane. Returns nonzero when placed. */
    int (*add_widget)(uint32_t widget_id);
    int (*add_widget_to_sidebar)(uint32_t sidebar_id, uint32_t widget_id, int index);
    /** Takes the widget out of its sidebar without closing it (CipSoft removeWidget). */
    void (*remove_widget)(uint32_t widget_id);
    /** `fn` runs for QTCLIENT_SIDEBARS_* events. */
    int (*subscribe_sidebars)(void (*fn)(void* user, int event, uint32_t a, uint32_t b), void* user);
    void (*unsubscribe_sidebars)(int id);

    /* --- sidebar widgets (CipSoft TSidebarWidget) --- */

    /** The widget of a Qt object: a widget item, its `widgetController`, or a drag source
     *  whose `widgetController` property names one. 0 when none. */
    uint32_t (*widget_from_object)(QtClientQObjectPtr object);
    /** The widget's item (null while it has none) and controller (the item's
     *  `widgetController`; give a detached item back to it as Qt object parent). */
    QtClientQQuickItemPtr (*widget_item)(uint32_t widget_id);
    QtClientQObjectPtr (*widget_controller)(uint32_t widget_id);
    /** The sidebar whose widget pane holds it, QTCLIENT_SIDEBAR_NONE when none. */
    uint32_t (*widget_parent_sidebar)(uint32_t widget_id);
    /** CipSoft resetParentSidebarId (the item is about to sit outside a widget pane). */
    void (*widget_reset_parent_sidebar)(uint32_t widget_id);
    void (*widget_maximize)(uint32_t widget_id);
    void (*widget_show)(uint32_t widget_id);
    void (*widget_hide)(uint32_t widget_id);
    /** CipSoft requestMoveToSidebar. */
    void (*widget_request_move_to_sidebar)(uint32_t widget_id, uint32_t sidebar_id);

    /** Widget kinds (CipSoft TSidebarWidgetsManager; pack kinds included). */
    int (*widget_kind_is_open)(int kind);
    void (*open_widget_kind)(int kind);
    /** The open widget of a kind (0 when closed; containers: use container_widgets). */
    uint32_t (*widget_of_kind)(int kind);
    /** CipSoft widgetTypeName / widgetTypeOf (pack kinds: their name_utf8); -1 unknown. */
    int (*widget_kind_name)(int kind, char* buf, size_t buf_len);
    int (*widget_kind_of_name)(const char* name_utf8);
    /** The open containers' widgets (list). */
    size_t (*container_widgets)(QtClientContainerWidget* out, size_t cap);

    /** Makes a pack widget (see QtClientSidebarWidgetDescV1) and registers its kind; not
     *  opened. Returns its widget id (0: failed, e.g. no game window or the item's QML has
     *  errors, logged CipSoft-style). One widget per kind. */
    uint32_t (*create_sidebar_widget)(const QtClientSidebarWidgetDescV1* desc);
    /** Makes the pack widget's item again from its qml_url when it was destroyed. Nonzero when
     *  it has an item afterwards. */
    int (*recreate_sidebar_widget_item)(uint32_t widget_id);
    /** The pack widget of `kind` closed itself (CipSoft onWidgetClosed: hides it, runs
     *  `closed`, emits WIDGET_HIDDEN). */
    void (*sidebar_widget_kind_closed)(int kind);
    /** Unregisters the kind and deletes the pack widget (call before `user` goes away). */
    void (*destroy_sidebar_widget)(uint32_t widget_id);

    /* --- the minimap panel (CipSoft TMinimapController) --- */

    /** The minimap controller, for QML only (e.g. a panel item's `panelController`). */
    QtClientQObjectPtr (*minimap_controller_object)(void);
    /** The minimap item (TMinimapQuickItem) inside the current panel, null when none. */
    QtClientQQuickItemPtr (*minimap_item)(void);
    /** The item the minimap controller drives instead of CipSoft's MinimapPanel. */
    void (*set_minimap_panel_item)(QtClientQQuickItemPtr item);
    /** What the minimap shows; nonzero when there is a minimap. */
    int (*minimap_view)(QtClientMinimapView* out);
    /** `fn` runs when the minimap view changes (CipSoft currentLayerChanged). */
    int (*subscribe_minimap_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_minimap_changed)(int id);
    /** The minimap files' directory (CipSoft TMinimapManager::minimapDir; sized out). Any
     *  thread. */
    int (*minimap_dir)(char* buf, size_t buf_len);

    /* --- the Cyclopedia map renderer (CipSoft TMinimapRendererQuickItem) --- */
    /* `renderer` is such an item in pack QML (import qmlcomponents); every call returns 0 / does
     * nothing for another item. Its bool properties (showZones, showHouses, showNPCMarkers,
     * showPassages, showExploreOverlay, showAreaCaptions) and minimapMarkersFilter are set
     * through the meta-object system directly. */

    int (*map_renderer_set_tile_type)(QtClientQQuickItemPtr renderer, int tile_type);
    int (*map_renderer_set_highlighted_subareas)(QtClientQQuickItemPtr renderer, const int* ids, size_t count);
    /** Copied. */
    int (*map_renderer_set_extra_zone_masks)(QtClientQQuickItemPtr renderer, const QtClientZoneMask* masks,
                                             size_t count);
    /** The camera (CipSoft TWorldMapCamera): the viewed layer, changing it with CipSoft's
     *  isometric shift, the zoom reference point (item pixels) and scale factor, centring a
     *  subfield (32 per field) once the item has a size. */
    int (*map_renderer_layer)(QtClientQQuickItemPtr renderer);
    int (*map_renderer_change_layer)(QtClientQQuickItemPtr renderer, int layer);
    int (*map_renderer_set_reference_point)(QtClientQQuickItemPtr renderer, int x, int y);
    int (*map_renderer_set_scale_factor)(QtClientQQuickItemPtr renderer, float scale);
    int (*map_renderer_center_delayed)(QtClientQQuickItemPtr renderer, int subfield_x, int subfield_y, int layer);
    /** The item pixel of field (x, y)'s top-left corner on `layer`. */
    int (*map_renderer_pixel_of)(QtClientQQuickItemPtr renderer, int x, int y, int layer, double* out_x,
                                 double* out_y);
    /** The field under item pixel (px, py) on the viewed layer. */
    int (*map_renderer_field_at)(QtClientQQuickItemPtr renderer, double px, double py, int* out_x, int* out_y);
    /** `fn` runs for QTCLIENT_MAP_RENDERER_* events of `renderer` until unsubscribed or the
     *  item is destroyed. */
    int (*map_renderer_subscribe)(QtClientQQuickItemPtr renderer,
                                  void (*fn)(void* user, int event, int a, int b), void* user);
    void (*map_renderer_unsubscribe)(int id);

    /* --- store images --- */

    /** The store image base URL the server sent (CipSoft TStoreProtocolMessageHandler
     *  imageBaseUrl; "" none; sized out). */
    int (*store_server_image_base)(char* buf, size_t buf_len);
    /** `fn` runs when the server sends a new one (and on each session change). */
    int (*subscribe_store_server_image_base)(void (*fn)(void* user, const char* url_utf8), void* user);
    void (*unsubscribe_store_server_image_base)(int id);
    /** The base URL store images load from (CipSoft TDynamicallyLoadedImageManager). */
    void (*set_store_image_base)(const char* url_utf8);

    /* --- sprites --- */

    /** Sprite `sprite_id`'s pixels (QTCLIENT_IMAGE_FORMAT_RGBA8888, logical size), decoding its
     *  sheet when it is not cached. Returns nonzero with `*out` filled (host-owned pixels: give
     *  them back with sprite_image_release); 0 when unknown or, with `*loading` set nonzero,
     *  when the sheet is still loading. Any thread; never per frame (cache in the pack). */
    int (*sprite_image)(uint32_t sprite_id, int* loading, QtClientImageView* out);
    /** Any thread. */
    void (*sprite_image_release)(QtClientImageView* image);
} QtClientUiApiV1;

#ifdef __cplusplus
}
#endif
