#pragma once

/// Convenience include for pack entry points.
///
/// One pack source tree builds two ways (host API v9):
/// - desktop runtime MODULE (mods/<id>/native/mod.{dylib,so,dll}, Qt shared with the tip):
///   the pack exports `qtclient_mod_register`;
/// - WASM static-ABI (the Qt kit is static, so a Qt pack cannot be a SIDE_MODULE): the same
///   sources, compiled against abi/ only, go into a static library linked into tibia.wasm,
///   which calls `qtclient_mod_<id>_register_abi` with the same arguments. The build defines
///   QTCLIENT_MOD_STATIC_ABI for that (qtclient_add_mod STATIC_ABI mode).
///
/// Write the entry once with the macro; `c_id` is the pack id as a C identifier
/// (mod.json "id" with '-' / '.' turned into '_'):
///
///     QTCLIENT_MOD_DEFINE_REGISTER(my_pack)(const QtClientHostApiV1* api, const QtClientModLoadCtx* ctx)
///     {
///         ...
///     }

#include "qtclient/abi/host_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#  define QTCLIENT_MOD_C_LINKAGE extern "C"
#else
#  define QTCLIENT_MOD_C_LINKAGE
#endif

/** The static-ABI entry's symbol for pack `c_id` (the tip's generated table names it). */
#define QTCLIENT_MOD_STATIC_ABI_SYMBOL(c_id) qtclient_mod_##c_id##_register_abi

#if defined(QTCLIENT_MOD_STATIC_ABI)
#  define QTCLIENT_MOD_DEFINE_REGISTER(c_id) QTCLIENT_MOD_C_LINKAGE void QTCLIENT_MOD_STATIC_ABI_SYMBOL(c_id)
#else
/** Pack MODULE entry — implement in pack code; tip resolves by name. */
QTCLIENT_MOD_EXPORT void qtclient_mod_register(const QtClientHostApiV1* api,
                                               const QtClientModLoadCtx* ctx);
#  define QTCLIENT_MOD_DEFINE_REGISTER(c_id) QTCLIENT_MOD_EXPORT void qtclient_mod_register
#endif

#ifdef __cplusplus
}
#endif
