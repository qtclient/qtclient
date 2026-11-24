#pragma once

#include "input/gameaction.h"

#include <cstdint>
#include <memory>

namespace tibia::game {
class TPlayerData;
}

namespace tibia::creatures {

class TCreatureStorage;

/// The party actions a creature offers (CIP: TPartyGameActionProvider), from
/// the local player's shield and the creature's.
class TPartyGameActionProvider
{
public:
    TPartyGameActionProvider(std::shared_ptr<TCreatureStorage> creatureStorage, std::shared_ptr<game::TPlayerData> playerData);
    virtual ~TPartyGameActionProvider() = default;

    /// Appends to `actions`: for another creature the invitation, join,
    /// revocation or leadership pass its shield allows and, for a party member,
    /// joining its aggression; for the local player in a party and out of
    /// fight, shared experience (leader only) and leaving the party.
    virtual void appendActions(input::TGameActionList& actions, uint32_t creatureId) const;

private:
    std::shared_ptr<TCreatureStorage> m_creatureStorage;
    std::shared_ptr<game::TPlayerData> m_playerData;
};

}
