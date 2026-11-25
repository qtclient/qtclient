#pragma once

#include "qtclient/abi/pack_host_api.h"

namespace tibia::mods {

/// The chrome table (pack_host_abi.cpp) over the game window's objects (abi_internal.h).
const QtClientPackHostApiV1* packHostAbiTable();
/// A new game window: the packs' options pages go into its options dialog.
void bindPackHostToHost();

} // namespace tibia::mods
