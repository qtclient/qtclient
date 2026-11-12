#pragma once

/// Client options store peels on the host-pushed pack ABI (JSON as UTF-8).

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*QtClientOptionChangedFn)(void* user, const char* name);

typedef struct QtClientOptionsApiV1 {
    /**
     * Load options section as JSON object text into *out_json (malloc'd UTF-8).
     * Caller frees with free_string. Returns 0 on success.
     */
    int (*load_options_json)(char** out_json);
    /** Store one option; value_json is a JSON value text (string/number/object/…). */
    int (*store)(const char* name, const char* value_json);
    void (*free_string)(char* p);

    int (*subscribe_changed)(QtClientOptionChangedFn fn, void* user);
    void (*unsubscribe_changed)(int id);
} QtClientOptionsApiV1;

#ifdef __cplusplus
}
#endif
