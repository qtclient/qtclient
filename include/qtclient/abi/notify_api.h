#pragma once

/// CipSoftHook-style field notify on the host-pushed pack ABI.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Simple field hook (args ignored) — covers most IdleOT SoftHook sites. */
typedef void (*QtClientNotifySimpleFn)(void* user);

/**
 * Field hook with opaque tip Args* (pack must not interpret layout).
 * Prefer typed channels when peeling specific payloads.
 */
typedef void (*QtClientNotifyFieldFn)(void* user, void* tip_args);

typedef struct QtClientNotifyApiV1 {
    int (*subscribe_simple)(const char* field, QtClientNotifySimpleFn fn, void* user);
    int (*subscribe_field)(const char* field, QtClientNotifyFieldFn fn, void* user);
    void (*unsubscribe)(int id);

    int (*subscribe_world_time)(void (*fn)(void* user, int hour, int minute), void* user);
    void (*unsubscribe_world_time)(int id);
    /** `fn` runs when the current game session disconnected, while it is still current. */
    int (*subscribe_session_end)(void (*fn)(void* user), void* user);
    void (*unsubscribe_session_end)(int id);

    /** Peel tip Args* (opaque) — UTF-8 into buf; returns length excl NUL, or -1. */
    int (*arg_string)(void* tip_args, size_t index, char* buf, size_t buf_len);
    int64_t (*arg_int)(void* tip_args, size_t index);
    int (*arg_bool)(void* tip_args, size_t index);

    /** Emit on the CIP field bus (e.g. onTextMessage): a0 = MessageMode as int64, a1 = UTF-8. */
    int (*emit_int_string)(const char* field, int64_t a0, const char* a1);

    /** Host API v5+ (read only when api_version >= 5): `fn` runs each time the game client's
     *  current session changes (a login, replay or tutorial began, or the session went away).
     *  session->container_storage() and the other current-session getters then return the new
     *  session's objects (or null). */
    int (*subscribe_session_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_session_changed)(int id);

    /** Host API v7+ (read only when api_version >= 7): `fn` runs when the current session's
     *  Team Finder storage changes (the own team's listing or roster, the listed teams) and
     *  on each session change. */
    int (*subscribe_team_finder_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_team_finder_changed)(int id);
} QtClientNotifyApiV1;

#ifdef __cplusplus
}
#endif
