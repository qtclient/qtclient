#pragma once

/// Pure C POD / handle types for the host-pushed pack ABI.
/// Packs may include this without linking tip or Qt.
///
/// Conventions every abi/ header follows (host API v9):
///
/// - Threading: every function and callback runs on the GUI thread (the thread of the
///   QGuiApplication and the QML engine) unless its comment says "Any thread".
/// - Strings: UTF-8, NUL-terminated. A `const char*` the host passes into a callback is
///   borrowed for the call; a `const char*` field of a host-filled struct stays valid until
///   the struct's owner says otherwise (documented per field).
/// - Sized out strings: `int f(..., char* buf, size_t buf_len)` writes at most
///   buf_len - 1 bytes plus a NUL and returns the full length without the NUL; a return
///   >= buf_len means the text was cut (call again with a larger buffer); buf may be null
///   when buf_len is 0 (length query); -1 means no value. JSON out values use the same rule.
///   (Entries from v1..v8 such as session->local_player_name / item_name and
///   notify->arg_string return the bytes written instead; their comments say so.)
/// - Lists: `size_t f(T* out, size_t cap)` fills at most `cap` rows and returns the total;
///   out may be null when cap is 0.
/// - Subscriptions: `int subscribe_*(fn, user)` returns an id > 0 (0: not subscribed); the
///   matching `unsubscribe_*(id)` may be called inside the callback and with an unknown id.
///   A callback never runs inside its own subscribe call.
/// - `void* user` / `void* self`: pack-owned context the host stores and hands back; the host
///   never dereferences it.
/// - Qt objects: a pointer to a Qt object travels as one of the QtClientQ*Ptr typedefs below
///   (void* on the C side; the ABI includes no Qt header). It always points at an instance of
///   the Qt class the typedef names (or a subclass); a pack uses only that Qt class's API on it
///   (properties, signals and invokables by name through the meta-object system) and never
///   casts it to a host class. No other ABI pointer may point at a host object: host objects
///   are reached through the typed tables, opaque handles (QtClientHandle, incomplete structs)
///   or ids.
/// - Pack-filled descriptor / vtable structs start with `size_t struct_size` (the pack sets
///   sizeof); host-filled tables added in v9 start with it too, so later fields can be added at
///   the tail and checked with QTCLIENT_ABI_HAS(table, Type, field). The host copies a
///   pack-filled descriptor when it takes it (the pack's struct may live on the stack); the
///   `user` pointer and function pointers in it must stay valid until the documented end.
/// - Errors: functions return 0 / -1 / null for "no", never abort; nothing throws across the
///   ABI (a pack must catch its C++ exceptions in every callback and in qtclient_mod_register).

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Host-opaque token (a host object the pack never dereferences or casts). */
typedef struct QtClientHandle {
    void* ptr;
} QtClientHandle;

/** Qt object pointers (see the conventions above). The name says the Qt class. */
typedef void* QtClientQObjectPtr;        /* QtCore's object base class (any Q_OBJECT class) */
typedef void* QtClientQQuickItemPtr;     /* QQuickItem */
typedef void* QtClientQQmlEnginePtr;     /* QQmlEngine */
typedef void* QtClientQQuickWindowPtr;   /* QQuickWindow */

/** Whether `table` (a `const Type*` whose first field is `size_t struct_size`) is long enough
 *  to hold `field`, e.g. QTCLIENT_ABI_HAS(api->ui, QtClientUiApiV1, map_renderer_layer). */
#define QTCLIENT_ABI_HAS(table, Type, field)                                                    \
    ((table) && (table)->struct_size >= offsetof(Type, field) + sizeof(((const Type*)0)->field))

/** A map position (CipSoft protocol coordinates). Moved here from session_api.h in v9. */
typedef struct QtClientMapPos {
    int x;
    int y;
    int z;
} QtClientMapPos;

typedef struct QtClientImageView {
    uint8_t* pixels;
    int width;
    int height;
    int bytes_per_line;
    /** QTCLIENT_IMAGE_FORMAT_*. */
    int format;
} QtClientImageView;

/** QtClientImageView.format. */
enum {
    /** RGBA bytes (premultiplied-agnostic raw). */
    QTCLIENT_IMAGE_FORMAT_RGBA8888 = 0,
    /** Native-endian 32-bit 0xAARRGGBB words, premultiplied (QImage::Format_ARGB32_Premultiplied):
     *  the world frame's light map handed to QtClientWorldFrameHooks.filter_light_map. */
    QTCLIENT_IMAGE_FORMAT_ARGB32_PREMULTIPLIED = 1,
};

typedef struct QtClientModIdentity {
    const char* id;
    const char* display_name;
    const char* organization;
    const char* organization_domain;
    const char* bundle_id;
    const char* cdn_base_url;
    int priority;
} QtClientModIdentity;

/** The game window context (abi/context.h). */
struct QtClientGameContextV1;

typedef struct QtClientModLoadCtx {
    const char* pack_id;
    const char* pack_dir;
    const char* host_build_id;
    int api_version;
    /** v9 (was `void* early_ctx`, the tip's ModContext*): the game window context while a
     *  bootstrap runs (register_mod / register_early_bootstrap), null during
     *  qtclient_mod_register. Borrowed for the call: copy what you keep. */
    const struct QtClientGameContextV1* game;
} QtClientModLoadCtx;

#ifdef __cplusplus
}
#endif
