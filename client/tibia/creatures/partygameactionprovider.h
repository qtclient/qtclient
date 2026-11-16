#pragma once

#include "input/gameaction.h"

#include <cstdint>

namespace tibia::creatures {

/// The party actions a creature offers (CIP: TPartyGameActionProvider), from
/// the local player's shield and the creature's.
class TPartyGameActionProvider
{
public:
    /// Appends to `actions`: for another creature the invitation, join,
    /// revocation or leadership pass its shield allows and, for a party member,
    /// joining its aggression; for the local player in a party and out of
    /// fight, shared experience (leader only) and leaving the party.
    static void appendActions(input::TGameActionList& actions, uint32_t creatureId);
};

}
