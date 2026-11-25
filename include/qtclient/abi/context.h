#pragma once

/// The game window context a bootstrap gets (host API v9; replaces the tip's ModContext that
/// QtClientModLoadCtx.early_ctx used to carry). Host: client/tibia/mods/context_abi.cpp.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * QtClientModLoadCtx.game while a bootstrap runs (null during qtclient_mod_register).
 *
 * Early bootstraps (register_early_bootstrap) run once the QML engine and the game client
 * exist, before the game window item is made: qml_engine, root_item, lifetime_parent and the
 * launcher urls are set; game_window_item and map_window_pane are null.
 * Mod bootstraps (register_mod) run once the game window item exists: every field is set.
 *
 * The struct and its strings are borrowed for the bootstrap call: copy what you keep. The Qt
 * objects it points at live until the application quits (lifetime_parent: until
 * QCoreApplication::aboutToQuit has been delivered).
 */
typedef struct QtClientGameContextV1 {
    size_t struct_size;
    /** The game window's QML engine (context properties, QQmlComponent of pack modules). */
    QtClientQQmlEnginePtr qml_engine;
    /** The window's root item. */
    QtClientQQuickItemPtr root_item;
    /** CipSoft's GameWindow item (null in an early bootstrap). */
    QtClientQQuickItemPtr game_window_item;
    /** The map window pane (MapWindowPane: the map plus the chrome drawn over it; its
     *  `mapWindowController` property is the CipSoft map window controller). Null early. */
    QtClientQQuickItemPtr map_window_pane;
    /** A host object that lives as long as the application: parent long-lived pack objects to
     *  it so they are deleted before Qt goes away. */
    QtClientQObjectPtr lifetime_parent;
    /** Launcher environment (the host's LauncherEnv, after pack mod.json `urls`): absolute
     *  URLs, "" when unset. */
    const char* login_base;
    const char* asset_base;
    const char* payments_base;
    /** The client revision sent to the login service ("" for dev builds). */
    const char* client_revision;
    /** Nonzero when the desktop launcher started the client. */
    int from_launcher;
} QtClientGameContextV1;

#ifdef __cplusplus
}
#endif
