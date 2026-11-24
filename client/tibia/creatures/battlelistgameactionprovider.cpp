#include "creatures/battlelistgameactionprovider.h"

#include "input/genericobjectgameactionprovider.h"

namespace tibia::creatures {

TBattleListGameActionProvider::TBattleListGameActionProvider(
    std::shared_ptr<input::TGenericObjectGameActionProvider> genericObjectGameActionProvider)
    : m_genericObjectGameActionProvider(std::move(genericObjectGameActionProvider))
{
}

input::TGameActionList TBattleListGameActionProvider::battleListGameActions(const uint32_t creatureId) const
{
    input::TGameActionList actions =
        m_genericObjectGameActionProvider ? m_genericObjectGameActionProvider->creatureActions(creatureId) : input::TGameActionList{};
    actions.push_back(input::makeGameAction(input::EGameAction::CONTEXT_MENU));
    return actions;
}

}
