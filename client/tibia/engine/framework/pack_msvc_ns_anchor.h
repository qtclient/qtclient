#pragma once

// Force-included on MSVC pack TUs after framework/pch.h (see QtClientMod.cmake).
//
// Pack TUs that pull session/game hubs need `tibia::container::TInventorySlot`
// established before those headers. Tip engine TUs get there via their include
// graph; pack TUs with only /FI pch.h do not (MSVC C3083/C2039 on
// ::tibia::container::TInventorySlot).
//
// Do NOT open empty `namespace tibia::container {}` here — MSVC keeps that
// empty declaration and later definitions do not merge (C2039 on TInventorySlot).

#include "container/containerciptypes.h"
