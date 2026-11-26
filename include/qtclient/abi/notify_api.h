#pragma once

/// CipSoftHook-style field notify on the host-pushed pack ABI.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A notify call's arguments (host-opaque, v9; was `void* tip_args`). Borrowed for the
 *  callback: read it with the arg_* functions during the call only. */
typedef struct QtClientNotifyArgs QtClientNotifyArgs;

/** Simple field hook (args ignored) — covers most pack notify sites. */
typedef void (*QtClientNotifySimpleFn)(void* user);

/** Field hook with the call's arguments (`args` never null; may hold none). */
typedef void (*QtClientNotifyFieldFn)(void* user, const QtClientNotifyArgs* args);

/** QtClientNotifyApiV1.arg_kind (v9). */
enum {
    QTCLIENT_NOTIFY_ARG_NONE = 0,   /* index out of range */
    QTCLIENT_NOTIFY_ARG_BOOL = 1,
    QTCLIENT_NOTIFY_ARG_INT = 2,    /* any integer or enum */
    QTCLIENT_NOTIFY_ARG_DOUBLE = 3,
    QTCLIENT_NOTIFY_ARG_STRING = 4,
    QTCLIENT_NOTIFY_ARG_JSON = 5,   /* a structured value: read it with arg_json */
    QTCLIENT_NOTIFY_ARG_OTHER = 6,  /* a value the host cannot hand out */
};

/** v9 container events (subscribe_containers). */
enum {
    QTCLIENT_CONTAINER_UPDATED = 1,     /* contents changed */
    QTCLIENT_CONTAINER_OPENED = 2,      /* a new container (CipSoft isNewContainer) */
    QTCLIENT_CONTAINER_REMOVED = 3,
};

typedef struct QtClientNotifyApiV1 {
    int (*subscribe_simple)(const char* field, QtClientNotifySimpleFn fn, void* user);
    int (*subscribe_field)(const char* field, QtClientNotifyFieldFn fn, void* user);
    void (*unsubscribe)(int id);

    int (*subscribe_world_time)(void (*fn)(void* user, int hour, int minute), void* user);
    void (*unsubscribe_world_time)(int id);
    /** `fn` runs when the current game session disconnected, while it is still current. */
    int (*subscribe_session_end)(void (*fn)(void* user), void* user);
    void (*unsubscribe_session_end)(int id);

    /** String argument `index` as UTF-8 into buf; returns the bytes written without the NUL
     *  ("" when it is no string), -1 for a null buf. */
    int (*arg_string)(const QtClientNotifyArgs* args, size_t index, char* buf, size_t buf_len);
    /** Integer / enum / bool argument (0 otherwise). */
    int64_t (*arg_int)(const QtClientNotifyArgs* args, size_t index);
    int (*arg_bool)(const QtClientNotifyArgs* args, size_t index);

    /** Emit on the CIP field bus (e.g. onTextMessage): a0 = CIP chat message mode (QTCLIENT_MESSAGE_*), a1 = UTF-8. */
    int (*emit_int_string)(const char* field, int64_t a0, const char* a1);

    /** Host API v5+ (read only when api_version >= 5): `fn` runs each time the game client's
     *  current session changes (a login, replay or tutorial began, or the session went away).
     *  The session table's current-session reads then see the new session. */
    int (*subscribe_session_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_session_changed)(int id);

    /** Host API v7+ (read only when api_version >= 7): `fn` runs when the current session's
     *  Team Finder storage changes (the own team's listing or roster, the listed teams) and
     *  on each session change. */
    int (*subscribe_team_finder_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_team_finder_changed)(int id);

    /* --- Host API v9 (read only when api_version >= 9) --- */

    /** The number of arguments. */
    size_t (*arg_count)(const QtClientNotifyArgs* args);
    /** What argument `index` holds (QTCLIENT_NOTIFY_ARG_*). */
    int (*arg_kind)(const QtClientNotifyArgs* args, size_t index);
    /** Any argument as JSON (sized out; -1 for NONE / OTHER). Structured values: a struct is
     *  an object keyed by its C++ member names as the host declares them (camelCase, e.g.
     *  {"bossRaceId": 1, "cooldownTime": 0}), a vector an array, a tuple an array of its
     *  elements, a string type a string, 64-bit integers numbers (exact up to 2^53).
     *  The host documents the payload of each structured field in include/qtclient/README.md (e.g.
     *  onBossCooldown, onUpdateBestiaryMonsterData, onUpdateBestiaryCharmsData,
     *  onParseCyclopediaTracker, onCyclopediaMapDiscoveryArea, onTeamFinderList,
     *  onTeamFinderLeader). */
    int (*arg_json)(const QtClientNotifyArgs* args, size_t index, char* buf, size_t buf_len);
    /** `fn` runs for the current session's container changes (QTCLIENT_CONTAINER_*), across
     *  sessions (CipSoft TContainerStorage containerUpdated / containerRemoved). */
    int (*subscribe_containers)(void (*fn)(void* user, int event, uint8_t container_id), void* user);
    void (*unsubscribe_containers)(int id);
    /** `fn` runs when the player changed the current session's quick-loot lists in CipSoft's
     *  UI (TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput), across
     *  sessions. */
    int (*subscribe_quick_loot_user_changed)(void (*fn)(void* user), void* user);
    void (*unsubscribe_quick_loot_user_changed)(int id);
} QtClientNotifyApiV1;

#ifdef __cplusplus
}
#endif
