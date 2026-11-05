#pragma once

// Force-included first on MSVC linked-pack TUs (see QtClientMod.cmake).
//
// Pack TUs lack engine /FI pch. Pre-parsing containerciptypes establishes
// `tibia::container::TInventorySlot` before pack/tip headers. Do NOT open
// empty `namespace tibia::container {}` here — MSVC then keeps that empty
// declaration and later definitions do not merge (C2039 on TInventorySlot).

#include "container/containerciptypes.h"
