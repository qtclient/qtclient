#pragma once

/// Registry / hook callbacks for the host-pushed pack ABI.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*QtClientModBootstrapFn)(const QtClientModLoadCtx* ctx);

typedef struct QtClientModEntry {
    QtClientModIdentity identity;
    QtClientModBootstrapFn bootstrap;
} QtClientModEntry;

typedef uint8_t (*QtClientEffectTintFn)(uint16_t effect_id);
typedef uint8_t (*QtClientEffectVariantTintFn)(uint32_t variant_id);
typedef void (*QtClientApplyImageTintFn)(QtClientImageView* image, uint8_t tint);

/** A walk target CipSoft's planner refused as out of range (destination_out_of_range).
 *  Return nonzero to take it over: the host then shows no failure message. */
typedef int (*QtClientWalkUnreachableFn)(void* user, int x, int y, int z, int reason);

typedef void (*QtClientLoginSyncEnqueueFn)(void (*fn)(void* user), void* user);
typedef void (*QtClientEnsureModsFn)(void);

/** HD scaler: write scaled pixels into *out (host frees previous out->pixels if non-null). */
typedef int (*QtClientHdScaleFn)(const QtClientImageView* src, QtClientImageView* out, void* user);

#ifdef __cplusplus
}
#endif
