#pragma once

/// Game host lifecycle and character switching (host API v9). Replaces the tip's
/// runtime::TGameHost and host::app() HD flag. Host: client/tibia/mods/gamehost_abi.cpp.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** QtClientGameHostApiV1.subscribe events. */
enum {
    /** The game window finished its setup for a new game (CipSoft main window initialised). */
    QTCLIENT_GAMEHOST_MAIN_WINDOW_INITIALIZED = 1,
    /** The player left the world (logout, death screen accepted, connection lost). */
    QTCLIENT_GAMEHOST_WORLD_LEFT = 2,
    /** The character selection was asked for (in game, to switch characters). */
    QTCLIENT_GAMEHOST_CHARACTER_SELECTION_REQUESTED = 3,
    /** That character selection closed. */
    QTCLIENT_GAMEHOST_CHARACTER_SELECTION_CLOSED = 4,
    /** A switch to another character began; `arg_utf8` is its name. */
    QTCLIENT_GAMEHOST_CHARACTER_SWITCH_REQUESTED = 5,
};

/** `arg_utf8` is "" unless the event documents it; borrowed for the call. */
typedef void (*QtClientGameHostEventFn)(void* user, int event, const char* arg_utf8);

typedef struct QtClientGameHostApiV1 {
    size_t struct_size;

    /** `fn` runs for `event` (QTCLIENT_GAMEHOST_*; 0 = every event). */
    int (*subscribe)(int event, QtClientGameHostEventFn fn, void* user);
    void (*unsubscribe)(int id);

    /** The account's character names (the character list of the last login) as a JSON array
     *  of strings, in the list's order (sized out). */
    int (*character_names_json)(char* buf, size_t buf_len);
    /** Switches to character `name_utf8` of the account (logout, then login with it).
     *  Returns nonzero when the switch started (unknown name, offline or a switch already
     *  running: 0). */
    int (*request_character_switch)(const char* name_utf8);
    /** Opens the character selection for a switch, or closes it when it shows. */
    void (*toggle_character_selection_for_switch)(void);

    /** Nonzero while the world map renderer exists (a game, replay or tutorial is shown). */
    int (*world_view_active)(void);

    /** The host's HD sprite mode (sprites scaled through set_hd_scaler). */
    int (*hd_mode)(void);
    /** Turns HD mode on / off; the sprite atlases are rebuilt. */
    void (*set_hd_mode)(int on);
} QtClientGameHostApiV1;

#ifdef __cplusplus
}
#endif
