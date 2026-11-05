#pragma once

#include "mods/registry.h"

/// Pack native entry — STATIC-linked into the tip. Each pack exports
/// `qtclient_mod_<id>_entry`. The tip generates `qtclient_invoke_linked_mods`
/// which calls every linked entry once at load. FS mods/<id>/ still supplies
/// QML / overrides / urls.
using QtClientModEntryFn = void (*)(const tibia::mods::ModContext* earlyCtx);

#define QTCLIENT_MOD_ENTRY_FN(id) qtclient_mod_##id##_entry

// C linkage only. Packs are STATIC — no Q_DECL_EXPORT / visibility attribute.
// Putting `extern "C"` and `__declspec(dllexport)` in one macro makes MSVC
// C2144 at `void` on pack TUs; GenerateLinkedMods already emits plain
// `extern "C" void qtclient_mod_<id>_entry`.
#define QTCLIENT_MOD_EXPORT extern "C"

#if defined(QTCLIENT_HAS_LINKED_PACK)
/// Tip-generated aggregator (see cmake/GenerateLinkedMods.cmake).
QTCLIENT_MOD_EXPORT void qtclient_invoke_linked_mods(const tibia::mods::ModContext* earlyCtx);
/// Back-compat alias → qtclient_invoke_linked_mods (single- or multi-pack tips).
QTCLIENT_MOD_EXPORT void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx);
#endif
