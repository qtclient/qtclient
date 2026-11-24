#pragma once

/// C lighting driver surface (host-pushed ABI). No Qt / tip types.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** CipSoft appearance flags of a type (TAppearanceType / appearances.proto names), in
 *  QtClientFieldStackEntry.flags, QtClientLightingThingContext.type_flags and
 *  QtClientLightingFrameDesc.type_flags. */
enum {
    QTCLIENT_APPEARANCE_PRESENT = 1u << 0,        /* there is a type (0: none) */
    QTCLIENT_APPEARANCE_OBJECT = 1u << 1,         /* object category (isItem) */
    QTCLIENT_APPEARANCE_OUTFIT = 1u << 2,         /* outfit category (isCreature) */
    QTCLIENT_APPEARANCE_BANK = 1u << 3,           /* isGround */
    QTCLIENT_APPEARANCE_CLIP = 1u << 4,           /* isGroundBorder */
    QTCLIENT_APPEARANCE_BOTTOM = 1u << 5,
    QTCLIENT_APPEARANCE_TOP = 1u << 6,
    QTCLIENT_APPEARANCE_UNPASS = 1u << 7,         /* isNotWalkable */
    QTCLIENT_APPEARANCE_UNMOVE = 1u << 8,         /* isNotMoveable */
    QTCLIENT_APPEARANCE_UNSIGHT = 1u << 9,        /* blockProjectile */
    QTCLIENT_APPEARANCE_AVOID = 1u << 10,         /* isNotPathable */
    QTCLIENT_APPEARANCE_FULL_BANK = 1u << 11,     /* isFullGround */
    QTCLIENT_APPEARANCE_DONT_HIDE = 1u << 12,
    QTCLIENT_APPEARANCE_TRANSLUCENT = 1u << 13,
    QTCLIENT_APPEARANCE_HANG = 1u << 14,          /* isHangable */
    QTCLIENT_APPEARANCE_ELEVATION = 1u << 15,     /* hasElevation */
    QTCLIENT_APPEARANCE_LIQUID_POOL = 1u << 16,   /* isSplash */
    QTCLIENT_APPEARANCE_LYING_OBJECT = 1u << 17,  /* isLyingCorpse */
    QTCLIENT_APPEARANCE_CORPSE = 1u << 18,
    QTCLIENT_APPEARANCE_PLAYER_CORPSE = 1u << 19,
    QTCLIENT_APPEARANCE_SINGLE_FIELD = 1u << 20,  /* isSingleDimension (1x1) */
    QTCLIENT_APPEARANCE_LIGHT = 1u << 21,         /* hasLight */
};

/** One entry of a map field's stack (CipSoft order: TWorldMapField::appearances). */
typedef struct QtClientFieldStackEntry {
    uint32_t flags;       /* QTCLIENT_APPEARANCE_* of the entry's type */
    uint32_t creature_id; /* nonzero: a creature's placeholder */
    uint16_t type_id;
    uint8_t height;       /* type size in fields (TAppearanceType::getHeight), 0..255 */
    uint8_t elevation;    /* TAppearanceType::getElevation, 0..255 */
} QtClientFieldStackEntry;

typedef struct QtClientLightingMapPos {
    int x;
    int y;
    int z;
} QtClientLightingMapPos;

typedef struct QtClientLightingThingContext {
    uint8_t lift;
    uint8_t creature;
    uint8_t walking;
    uint8_t local_player;
    uint8_t loose;
    uint8_t transient;
    /// Stable creature id for stamp/light identity (0 when not a creature).
    uint32_t creature_id;
    /// v8: the drawn object's type and its QTCLIENT_APPEARANCE_* flags (0 for a creature or a
    /// transient). Who casts a height shadow is the driver's: the host no longer decides it.
    uint32_t type_flags;
    uint16_t type_id;
    /// v8: its place in the stack of the field begin_field named last; -1 when it is drawn on
    /// a field it does not stand on (a stepping creature).
    int16_t stack_index;
} QtClientLightingThingContext;

typedef struct QtClientLightingFrameDesc {
    uint32_t sprite_id;
    int anchor_x;
    int anchor_y;
    int displacement_x;
    int displacement_y;
    uint8_t elevation;
    int8_t hook;
    uint8_t on_top;
    uint8_t lying_corpse;
    uint8_t has_light;
    uint8_t light_intensity;
    uint8_t light_color;
    /// v8: the frame's type and its QTCLIENT_APPEARANCE_* flags (the driver reads its shape
    /// and shadow from them).
    uint32_t type_flags;
    uint16_t type_id;
} QtClientLightingFrameDesc;

/** The field the world build draws next (v8, QtClientLightingDriver.begin_field). */
typedef struct QtClientLightingField {
    int ix; /* draw-grid cell, as reset_shade */
    int iy;
    QtClientLightingMapPos pos;
    const QtClientFieldStackEntry* stack; /* the field's stack, borrowed for the call */
    int stack_count;
} QtClientLightingField;

typedef struct QtClientLightingDriver {
    void* self;
    int (*enabled)(void* self);
    int (*frame_active)(void* self);
    int (*has_field_ambient)(void* self);
    int64_t (*gather_ns)(void* self);
    int64_t (*roofs_ns)(void* self);
    /// Ambient/classic views are borrowed for the duration of the call only; the pack must
    /// deep-own (copy) pixels before returning.
    void (*set_field_ambient)(void* self, QtClientImageView ambient);
    void (*set_field_classic)(void* self, QtClientImageView classic);
    void (*clear_session)(void* self);
    int (*begin_frame)(void* self, QtClientLightingMapPos camera, int draw_w, int draw_h,
                       int first_floor, int last_floor);
    void (*begin_floor)(void* self, int z);
    void (*reset_shade)(void* self, int ix, int iy, int can_shade);
    void (*end_frame)(void* self);
    void (*add_center_light)(void* self, int x, int y, uint8_t intensity, uint8_t color);
    void (*add_frame)(void* self, const QtClientLightingFrameDesc* frame);
    int (*stamp_phase)(void* self, int animation_phase);
    void (*begin_context)(void* self, const QtClientLightingThingContext* ctx);
    void (*end_context)(void* self);
    /** v8: the field whose things the build draws next (its stack); called per field while the
     *  driver gathers (begin_frame returned nonzero). May be null. */
    void (*begin_field)(void* self, const QtClientLightingField* field);
    /** v8: nonzero while the driver draws the frame's light itself; CipSoft's light map is then
     *  not drawn over it. May be null (never). */
    int (*replaces_classic_light)(void* self);
} QtClientLightingDriver;

/// QSG world overlay after sprites. `window` / `camera` / `after_sprites` are QQuickWindow* / QSGNode*;
/// pack uses QtClientHandle so the ABI stays POD-only.
typedef struct QtClientWorldOverlayFactory {
    void* self;
    int (*order)(void* self);
    void (*update_scene)(void* self, QtClientHandle window, QtClientHandle camera,
                         QtClientHandle after_sprites);
    void (*abandon_node)(void* self);
    void (*drop_node)(void* self);
} QtClientWorldOverlayFactory;

/** What a world frame hook reads about the frame the host built (v8). */
typedef struct QtClientWorldFrameView {
    int valid;                     /* 0: no world this frame (no player / offline); the rest is 0 */
    QtClientLightingMapPos camera; /* the player's field */
    int draw_width;                /* walked fields (visible + 3) */
    int draw_height;
    int center_x;                  /* the camera's cell in the draw grid */
    int center_y;
    int first_floor;               /* visible floors, inclusive (first <= last) */
    int last_floor;
    uint32_t item_revision;        /* the map's object revision (changes with any object) */
    uint64_t clock_start_millis;   /* the frame clock's start (session millis() minus this = time since start) */
} QtClientWorldFrameView;

typedef void (*QtClientWorldFrameFn)(void* user, const QtClientWorldFrameView* view);
/** `light` is the frame's light map (QTCLIENT_IMAGE_FORMAT_ARGB32_PREMULTIPLIED, one pixel per
 *  walked field), writable in place for the call; the lighting driver's set_field_classic then
 *  gets the filtered one, set_field_ambient the unfiltered one. */
typedef void (*QtClientLightMapFilterFn)(void* user, const QtClientWorldFrameView* view, QtClientImageView* light);

/** v8: an extension's hooks on the live world's frame build (none on preview volumes). */
typedef struct QtClientWorldFrameHooks {
    void* user;
    /** After the sprite build, before the light map; once with valid = 0 when a build finds no
     *  world. May be null. */
    QtClientWorldFrameFn after_build;
    /** The light map before it is drawn (frames with a light map only). May be null. */
    QtClientLightMapFilterFn filter_light_map;
} QtClientWorldFrameHooks;

#ifdef __cplusplus
}
#endif
