#pragma once

#include "input/gameaction.h"

#include <cstdint>

namespace tibia::creatures {

/// The inspection actions a player offers (CIP: TInspectPlayerGameActionProvider).
class TInspectPlayerGameActionProvider
{
public:
    /// Appends to `actions`: the local player inspects itself; another player
    /// is inspected or asked to allow it, then, unless everyone may inspect
    /// the local player, allowed (when it asks), or invited or revoked.
    static void appendActions(input::TGameActionList& actions, uint32_t creatureId);
};

}
