#include "creatures/inspectplayergameactionprovider.h"

#include "app/clientoptions.h"
#include "creatures/creaturestorage.h"

namespace tibia::creatures {

void TInspectPlayerGameActionProvider::appendActions(input::TGameActionList& actions, const uint32_t creatureId)
{
    using input::EGameAction;
    TCreatureStorage* storage = TCreatureStorage::instance();
    const std::shared_ptr<TCreature> creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature || !creature->isPlayer())
        return;
    const auto push = [&](const EGameAction type) { actions.push_back(input::makeCreatureAction(type, creatureId)); };
    if (creature->isLocalPlayer()) {
        push(EGameAction::INSPECT_PLAYER);
        return;
    }
    push(creature->canBeInspected() ? EGameAction::INSPECT_PLAYER : EGameAction::ASK_TO_INSPECT_PLAYER);
    // Option allowAllToInspectPlayer: nobody needs an invitation.
    if (app::ClientOptions::load().options().value(QLatin1String("inspectPlayerAllowAllEnabled")).toBool())
        return;
    if (creature->requestsInspection()) {
        push(EGameAction::ALLOW_INSPECT_PLAYER);
        return;
    }
    push(creature->inspectionGranted() ? EGameAction::REVOKE_INSPECT_PLAYER : EGameAction::INVITE_TO_INSPECT_PLAYER);
}

}
