#pragma once

#include "mods/registry.h"

#include <QtGlobal>

#if defined(_WIN32)
#  define QTCLIENT_MOD_EXPORT extern "C" Q_DECL_EXPORT
#else
#  define QTCLIENT_MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

/// Pack native entry. Same symbol on desktop shared libs and wasm SIDE_MODULE.
/// Called once after QLibrary::load / dlopen — register mods, drivers, overlays here.
using QtClientModEntryFn = void (*)(const tibia::mods::ModContext* earlyCtx);

#define QTCLIENT_MOD_ENTRY_NAME "qtclient_mod_entry"
