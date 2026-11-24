#pragma once

#include "input/gameaction.h"

#include <cstdint>
#include <memory>

namespace tibia::config {
class TClientOptions;
}

namespace tibia::creatures {

class TCreatureStorage;

/// The inspection actions a player offers (CIP: TInspectPlayerGameActionProvider).
class TInspectPlayerGameActionProvider
{
public:
    TInspectPlayerGameActionProvider(std::shared_ptr<TCreatureStorage> creatureStorage, const config::TClientOptions* options);
    virtual ~TInspectPlayerGameActionProvider() = default;

    /// Appends to `actions`: the local player inspects itself; another player
    /// is inspected or asked to allow it, then, unless everyone may inspect
    /// the local player, allowed (when it asks), or invited or revoked.
    virtual void appendActions(input::TGameActionList& actions, uint32_t creatureId) const;

private:
    std::shared_ptr<TCreatureStorage> m_creatureStorage;
    const config::TClientOptions* m_options = nullptr;
};

}
