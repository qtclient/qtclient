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

typedef void (*QtClientApplyImageTintFn)(QtClientImageView* image, uint8_t tint);

/** A walk target CipSoft's planner refused as out of range (destination_out_of_range).
 *  Return nonzero to take it over: the host then shows no failure message. */
typedef int (*QtClientWalkUnreachableFn)(void* user, int x, int y, int z, int reason);

typedef void (*QtClientLoginSyncEnqueueFn)(void (*fn)(void* user), void* user);

/** HD scaler: write scaled pixels into *out (host frees previous out->pixels if non-null). */
typedef int (*QtClientHdScaleFn)(const QtClientImageView* src, QtClientImageView* out, void* user);

/** Texture atlas hint (set_texture_atlas_hint): asked once while the host sets up graphics,
 *  before any window. `*default_side` (4096: the atlas side when the client settings store
 *  none; 512 at least) and `*size_limit_cap` (256: the largest texture side that goes into
 *  the atlas) come in as CipSoft's and may be raised. */
typedef void (*QtClientTextureAtlasHintFn)(int* default_side, int* size_limit_cap);

/** What an extension draws for one server magic effect id (set_effect_remap_hook). */
typedef struct QtClientEffectRemap {
    /** The effect appearance id drawn instead (effect category). 0 leaves the id as it is. */
    uint16_t draw_id;
    /** Tint key handed to the apply_image_tint hook when the sprite texture is made; 0 none. */
    uint8_t tint;
    /** Nonzero: drawn in the creature layer of its field instead of on top of it. */
    uint8_t under_creatures;
} QtClientEffectRemap;

/** Asked for every server magic effect (add and remove messages). Return nonzero after
 *  filling *out to draw another effect; zero draws the id itself (CipSoft's behaviour).
 *  GUI thread. */
typedef int (*QtClientEffectRemapFn)(void* user, uint16_t effect_id, QtClientEffectRemap* out);

/** One sprite an extension draws with a creature (set_creature_decoration_hook). */
typedef struct QtClientCreatureDecoration {
    uint16_t appearance_id;
    /** QTCLIENT_THING_CATEGORY_* of appearance_id. */
    uint8_t category;
    /** Tint key handed to the apply_image_tint hook; 0 none. */
    uint8_t tint;
    /** Pixels from the creature's drawn field corner. */
    int16_t offset_x;
    int16_t offset_y;
    /** Animation phase; QTCLIENT_DECORATION_PHASE_ANIMATED (< 0) = the appearance's own
     *  animation, every decoration of an appearance in step. */
    int32_t phase;
    int32_t pattern_x;
    /** 0..1. */
    float opacity;
} QtClientCreatureDecoration;

/** Fill up to `cap` decorations of creature `creature_id` for `layer`
 *  (QTCLIENT_CREATURE_LAYER_*) and return how many were written. Called on the GUI thread
 *  while the map frame is built, twice per drawn creature: no allocation, no locks.
 *  The host passes cap = 16. */
typedef size_t (*QtClientCreatureDecorationFn)(void* user, uint32_t creature_id, int layer,
                                               QtClientCreatureDecoration* out, size_t cap);

#ifdef __cplusplus
}
#endif
