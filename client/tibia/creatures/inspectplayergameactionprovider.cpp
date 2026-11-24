#include "creatures/inspectplayergameactionprovider.h"

#include "config/clientoptions.h"
#include "creatures/creaturestorage.h"

namespace tibia::creatures {

TInspectPlayerGameActionProvider::TInspectPlayerGameActionProvider(std::shared_ptr<TCreatureStorage> creatureStorage,
                                                                   const config::TClientOptions* options)
    : m_creatureStorage(std::move(creatureStorage))
    , m_options(options)
{
}

void TInspectPlayerGameActionProvider::appendActions(input::TGameActionList& actions, const uint32_t creatureId) const
{
    using input::EGameAction;
    const std::shared_ptr<TCreature> creature = m_creatureStorage ? m_creatureStorage->creatureAt(creatureId) : nullptr;
    if (!creature || !creature->isPlayer())
        return;
    const auto push = [&](const EGameAction type) { actions.push_back(input::makeCreatureAction(type, creatureId)); };
    if (creature->isLocalPlayer()) {
        push(EGameAction::INSPECT_PLAYER);
        return;
    }
    push(creature->canBeInspected() ? EGameAction::INSPECT_PLAYER : EGameAction::ASK_TO_INSPECT_PLAYER);
    // Everyone may inspect the local player: nobody needs an invitation.
    if (m_options && m_options->boolOption(config::EClientOption::inspectPlayerAllowAllEnabled))
        return;
    if (creature->requestsInspection()) {
        push(EGameAction::ALLOW_INSPECT_PLAYER);
        return;
    }
    push(creature->inspectionGranted() ? EGameAction::REVOKE_INSPECT_PLAYER : EGameAction::INVITE_TO_INSPECT_PLAYER);
}

}
