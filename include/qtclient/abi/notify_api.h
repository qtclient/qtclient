#pragma once

/// CIP SoftHook-style field notify on the host-pushed pack ABI.

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
    int (*subscribe_session_end)(void (*fn)(void* user), void* user);
    void (*unsubscribe_session_end)(int id);

    /** Peel tip Args* (opaque) — UTF-8 into buf; returns length excl NUL, or -1. */
    int (*arg_string)(void* tip_args, size_t index, char* buf, size_t buf_len);
    int64_t (*arg_int)(void* tip_args, size_t index);
    int (*arg_bool)(void* tip_args, size_t index);

    /** Emit on the CIP field bus (e.g. onTextMessage): a0 = MessageMode as int64, a1 = UTF-8. */
    int (*emit_int_string)(const char* field, int64_t a0, const char* a1);
} QtClientNotifyApiV1;

#ifdef __cplusplus
}
#endif
