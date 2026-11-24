#pragma once

#include "input/gameaction.h"

#include <cstdint>
#include <memory>

namespace tibia::input {
class TGenericObjectGameActionProvider;
}

namespace tibia::creatures {

/// The actions a battle list entry offers (CIP: TBattleListGameActionProvider):
/// the generic object provider's creature actions, then CONTEXT_MENU.
class TBattleListGameActionProvider
{
public:
    explicit TBattleListGameActionProvider(std::shared_ptr<input::TGenericObjectGameActionProvider> genericObjectGameActionProvider);
    virtual ~TBattleListGameActionProvider() = default;

    /// The actions of the entry of creature `creatureId`.
    virtual input::TGameActionList battleListGameActions(uint32_t creatureId) const;

private:
    std::shared_ptr<input::TGenericObjectGameActionProvider> m_genericObjectGameActionProvider;
};

}
