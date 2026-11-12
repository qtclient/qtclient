#pragma once

#include "mods/registry.h"

/// Pack C++ is absorbed into the tip (`target_sources(tibia)`). Each pack
/// defines `qtclient_mod_<id>_register` (C++). Tip-generated `linked_mods.cpp`
/// owns `extern "C"` `qtclient_mod_<id>_entry` + `qtclient_invoke_linked_mods`.
/// FS mods/<id>/ still supplies QML / overrides / urls.
using QtClientModEntryFn = void (*)(const tibia::mods::ModContext* earlyCtx);

#define QTCLIENT_MOD_ENTRY_FN(id) qtclient_mod_##id##_entry
#define QTCLIENT_MOD_REGISTER_FN(id) qtclient_mod_##id##_register

#if defined(QTCLIENT_HAS_LINKED_PACK)
/// Tip-generated aggregator (see cmake/GenerateLinkedMods.cmake).
extern "C" void qtclient_invoke_linked_mods(const tibia::mods::ModContext* earlyCtx);
/// Back-compat alias → qtclient_invoke_linked_mods (single- or multi-pack tips).
extern "C" void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx);
#endif
