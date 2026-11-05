#pragma once

#include "mods/registry.h"

/// Pack native entry — STATIC-linked into the tip. Each pack exports
/// `qtclient_mod_<id>_entry`. The tip generates `qtclient_invoke_linked_mods`
/// which calls every linked entry once at load. FS mods/<id>/ still supplies
/// QML / overrides / urls.
using QtClientModEntryFn = void (*)(const tibia::mods::ModContext* earlyCtx);

#define QTCLIENT_MOD_ENTRY_FN(id) qtclient_mod_##id##_entry

// Optional ELF visibility. Do NOT put `extern "C"` in this macro — MSVC C2144
// when `extern "C"` arrives only via macro expansion before `void` (pack entry
// TUs). Write `extern "C"` at the call site (GenerateLinkedMods already does).
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
#  define QTCLIENT_MOD_EXPORT __attribute__((visibility("default")))
#else
#  define QTCLIENT_MOD_EXPORT
#endif

#if defined(QTCLIENT_HAS_LINKED_PACK)
/// Tip-generated aggregator (see cmake/GenerateLinkedMods.cmake).
extern "C" QTCLIENT_MOD_EXPORT void qtclient_invoke_linked_mods(const tibia::mods::ModContext* earlyCtx);
/// Back-compat alias → qtclient_invoke_linked_mods (single- or multi-pack tips).
extern "C" QTCLIENT_MOD_EXPORT void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx);
#endif
