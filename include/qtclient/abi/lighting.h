#pragma once

/// C lighting driver surface (host-pushed ABI). No Qt / tip types.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    uint8_t caster;
} QtClientLightingThingContext;

typedef struct QtClientLightingFrameDesc {
    uint32_t sprite_id;
    int anchor_x;
    int anchor_y;
    int displacement_x;
    int displacement_y;
    uint8_t shape;
    uint8_t elevation;
    int8_t hook;
    uint8_t on_top;
    uint8_t casts_shadow;
    uint8_t lying_corpse;
    uint8_t has_light;
    uint8_t light_intensity;
    uint8_t light_color;
} QtClientLightingFrameDesc;

typedef struct QtClientLightingDriver {
    void* self;
    int (*enabled)(void* self);
    int (*frame_active)(void* self);
    int (*has_field_ambient)(void* self);
    int64_t (*gather_ns)(void* self);
    int64_t (*roofs_ns)(void* self);
    void (*set_field_ambient)(void* self, QtClientImageView ambient);
    void (*set_field_classic)(void* self, QtClientImageView classic);
    void (*clear_session)(void* self);
    int (*begin_frame)(void* self, QtClientLightingMapPos camera, int draw_w, int draw_h,
                       int first_floor, int last_floor);
    void (*begin_floor)(void* self, int z);
    void (*reset_shade)(void* self, int ix, int iy, int can_shade);
    void (*end_frame)(void* self, void* world_geometry);
    void (*add_center_light)(void* self, int x, int y, uint8_t intensity, uint8_t color);
    void (*add_frame)(void* self, const QtClientLightingFrameDesc* frame);
    int (*stamp_phase)(void* self, int animation_phase);
    void (*begin_context)(void* self, const QtClientLightingThingContext* ctx);
    void (*end_context)(void* self);
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

typedef struct QtClientWorldFrameHooks {
    int request_outdoor_gather;
} QtClientWorldFrameHooks;

#ifdef __cplusplus
}
#endif
