#pragma once

/// Game actions, context menu extensions and target selection (host API v9). Replaces the
/// tip's IGameActionPtr / TInternalGameActionRouter / TContextMenuController /
/// input::startTargetSelection / TMouseCursorController for packs.
/// Host: client/tibia/mods/action_abi.cpp.
///
/// Game actions are named by CipSoft's EGameAction enumerator names ("ATTACK_NEXT_TARGET",
/// the table --smoke-action maps). Payloads are JSON objects (UTF-8). Actions with a payload:
///
///   OPEN_CYCLOPEDIA               {"tab": int, "raceId": int (optional)}
///   OPEN_TASKBOARD_DIALOG         {"tab": int}
///   OPEN_SOCIAL_DIALOG            {"tab": int}            (CipSoft's parameter, 0 = friend list)
///   FRIENDS_DIALOG_SEARCH_ACCOUNT {"characterName": string}
///   SET_STATUS_LINE               {"text": string}
///   START_TALKING_TO_NPC          {"creatureId": int}
///   SEND_CHAT_MESSAGE_TO_NPC      {"text": string, "name": string, "level": int}
///                                 (name / level: the speaker shown, the local player's by
///                                 default)
///
/// Every other action is payload-less: publish it with null or "{}"; subscribers get "{}".
/// The host may add payload keys later; readers ignore unknown keys.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Where an object of a game action is (CipSoft EGameActionContextType, simplified). */
enum {
    QTCLIENT_LOCATION_UNKNOWN = 0,
    QTCLIENT_LOCATION_MAP = 1,
    QTCLIENT_LOCATION_CONTAINER = 2,
    QTCLIENT_LOCATION_INVENTORY = 3,
};

/** An object a game action is about (CipSoft TObjectGameActionContext as plain data). */
typedef struct QtClientObjectContext {
    /** QTCLIENT_LOCATION_*. */
    int location;
    /** CipSoft's protocol position: a map field, or x = 0xFFFF with y = container id | 0x40
     *  (z = slot) for a container, y = inventory slot for the inventory. */
    QtClientMapPos pos;
    /** Stack position of the object on its field / slot, -1 when unknown. */
    int stack_pos;
    /** The object's appearance type id (0 for a creature). */
    uint16_t type_id;
    /** The creature (0 for an object). */
    uint32_t creature_id;
} QtClientObjectContext;

/** A context menu being built or shown (host-opaque; valid for the documented call or until
 *  show_custom_menu). */
typedef struct QtClientContextMenu QtClientContextMenu;

enum {
    /** context_menu_add_entry flags. */
    QTCLIENT_MENU_ENTRY_CHECKABLE = 1 << 0,
    QTCLIENT_MENU_ENTRY_CHECKED = 1 << 1,
    QTCLIENT_MENU_ENTRY_DISABLED = 1 << 2,
};

/** Returns nonzero to take the action over (CipSoft's handlers then skip it). */
typedef int (*QtClientGameActionFn)(void* user, const char* name, const char* payload_json);

/** Adds the extension's entries to a game-action context menu the host is building:
 *  `objects` (borrowed) are the object contexts of the menu's game actions, in order;
 *  `mouse_button` the Qt::MouseButton that opened it. Add entries to `menu` during the call
 *  only. */
typedef void (*QtClientContextMenuExtensionFn)(void* user, const QtClientObjectContext* objects, size_t count,
                                               int mouse_button, QtClientContextMenu* menu);

/** The selected target (`target` borrowed; null: the selection was cancelled). */
typedef void (*QtClientTargetSelectedFn)(void* user, const QtClientObjectContext* target);

typedef struct QtClientActionApiV1 {
    size_t struct_size;

    /* --- game actions --- */

    /** Publishes the game action through the game client's router, as CipSoft's controllers
     *  do. Returns 0 published, -1 unknown name / bad payload, -2 no game window yet. */
    int (*publish)(const char* name, const char* payload_json);
    /** `fn` runs for each routed action `name` (null / "": every action) before CipSoft's
     *  handlers; the first subscriber returning nonzero takes it over. */
    int (*subscribe)(const char* name, QtClientGameActionFn fn, void* user);
    void (*unsubscribe)(int id);

    /* --- context menus --- */

    /** `fn` runs for each game-action context menu (CipSoft addGameActionsExtension); entries
     *  it adds go after CipSoft's. */
    int (*add_context_menu_extension)(QtClientContextMenuExtensionFn fn, void* user);
    void (*remove_context_menu_extension)(int id);
    /** Starts a custom context menu at the mouse (CipSoft beginCustomMenu); add entries, then
     *  show_custom_menu. Null when there is no game window. */
    QtClientContextMenu* (*begin_custom_menu)(void);
    /** Shows the custom menu; `menu` is then invalid. */
    void (*show_custom_menu)(QtClientContextMenu* menu);
    /** Adds an entry (QTCLIENT_MENU_ENTRY_* flags); `triggered` runs when it is chosen (may be
     *  null); `user` must stay valid until the menu closed (`closed` below runs per entry). */
    void (*context_menu_add_entry)(QtClientContextMenu* menu, const char* label_utf8, int flags,
                                   void (*triggered)(void* user), void (*closed)(void* user), void* user);
    void (*context_menu_add_separator)(QtClientContextMenu* menu);

    /* --- target selection --- */

    /** Starts CipSoft's crosshair target selection: the next map / container / inventory
     *  click selects. `fn` runs once (also with null when cancelled or replaced).
     *  `set_cursor` nonzero also shows CipSoft's target cursor (TMouseCursorController
     *  setTargetSelection) until it ends. */
    void (*start_target_selection)(QtClientTargetSelectedFn fn, void* user, int set_cursor);
    /** Ends a running selection as cancelled. */
    void (*cancel_target_selection)(void);
    int (*is_selecting_target)(void);
} QtClientActionApiV1;

#ifdef __cplusplus
}
#endif
