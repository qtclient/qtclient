#pragma once

#include "qtclient/abi/pack_host_api.h"

namespace qtclient {
struct PackHostWire;
}

namespace tibia::mods {

/// Fills the process-global C ABI table from tip `PackHostWire` (call before `bootstrapAll`).
void fillPackHostAbiFromPackHostWire(qtclient::PackHostWire* wire);

const QtClientPackHostApiV1* packHostAbiTable();

} // namespace tibia::mods
