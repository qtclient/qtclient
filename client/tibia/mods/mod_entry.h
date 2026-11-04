#pragma once

#include "mods/registry.h"

#include <QtGlobal>

/// Pack native entry — STATIC-linked into the tip. Each pack exports
/// `qtclient_mod_<id>_entry`. The tip generates `qtclient_invoke_linked_mods`
/// which calls every linked entry once at load. FS mods/<id>/ still supplies
/// QML / overrides / urls.
using QtClientModEntryFn = void (*)(const tibia::mods::ModContext* earlyCtx);

#define QTCLIENT_MOD_ENTRY_FN(id) qtclient_mod_##id##_entry

#if defined(_WIN32)
#  define QTCLIENT_MOD_EXPORT extern "C" Q_DECL_EXPORT
#else
#  define QTCLIENT_MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(QTCLIENT_HAS_LINKED_PACK)
/// Tip-generated aggregator (see cmake/GenerateLinkedMods.cmake).
QTCLIENT_MOD_EXPORT void qtclient_invoke_linked_mods(const tibia::mods::ModContext* earlyCtx);
/// Back-compat alias → qtclient_invoke_linked_mods (single- or multi-pack tips).
QTCLIENT_MOD_EXPORT void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx);
#endif
