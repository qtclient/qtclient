#pragma once

/// Pure C POD / handle types for the host-pushed pack ABI.
/// Packs may include this without linking tip or Qt.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QtClientHandle {
    void* ptr;
} QtClientHandle;

typedef struct QtClientImageView {
    uint8_t* pixels;
    int width;
    int height;
    int bytes_per_line;
    /** 0 = RGBA8888 premultiplied-agnostic raw; other values reserved. */
    int format;
} QtClientImageView;

typedef struct QtClientModIdentity {
    const char* id;
    const char* display_name;
    const char* organization;
    const char* organization_domain;
    const char* bundle_id;
    const char* cdn_base_url;
    int priority;
} QtClientModIdentity;

typedef struct QtClientModLoadCtx {
    const char* pack_id;
    const char* pack_dir;
    const char* host_build_id;
    int api_version;
    /** Opaque tip ModContext* when bootstrap runs; null during register-only load. */
    void* early_ctx;
} QtClientModLoadCtx;

#ifdef __cplusplus
}
#endif
