#include "creatures/partygameactionprovider.h"

#include "creatures/creaturestorage.h"
#include "game/playerdata.h"

#include <tibia/game/gamesession.h>

namespace tibia::creatures {

namespace {

using input::EGameAction;

/// The shield-to-shield offer to another creature; UNKNOWN when there is none.
EGameAction shieldAction(const uint8_t own, const uint8_t other)
{
    if (own > 10)
        return EGameAction::UNKNOWN;
    // A leader (yellow shields) invites, revokes an invitation or passes the lead.
    if ((0x550u >> own & 1) != 0) {
        if (other > 11)
            return EGameAction::INVITE_TO_PARTY;
        if ((0x2a8u >> other & 1) != 0)
            return EGameAction::PASS_PARTY_LEADERSHIP;
        if (other == 2)
            return EGameAction::REVOKE_PARTY_INVITATION;
        return other == 11 ? EGameAction::UNKNOWN : EGameAction::INVITE_TO_PARTY;
    }
    // Without a party (or only invited) one joins the inviting leader or invites.
    if (own == 0 || own == 2) {
        if (other == 1)
            return EGameAction::JOIN_PARTY;
        return other == 11 ? EGameAction::UNKNOWN : EGameAction::INVITE_TO_PARTY;
    }
    if (own == 1 && other == 2)
        return EGameAction::REVOKE_PARTY_INVITATION;
    return EGameAction::UNKNOWN;
}

}

TPartyGameActionProvider::TPartyGameActionProvider(std::shared_ptr<TCreatureStorage> creatureStorage,
                                                   std::shared_ptr<game::TPlayerData> playerData)
    : m_creatureStorage(std::move(creatureStorage))
    , m_playerData(std::move(playerData))
{
}

void TPartyGameActionProvider::appendActions(input::TGameActionList& actions, const uint32_t creatureId) const
{
    if (!m_creatureStorage)
        return;
    const std::shared_ptr<TCreature> player = m_creatureStorage->creatureAt(game::localPlayerId());
    const std::shared_ptr<TCreature> creature = m_creatureStorage->creatureAt(creatureId);
    if (!player || !creature)
        return;
    const auto push = [&](const EGameAction type) { actions.push_back(input::makeCreatureAction(type, creatureId)); };

    if (!creature->isLocalPlayer()) {
        if (const EGameAction type = shieldAction(player->shield(), creature->shield()); type != EGameAction::UNKNOWN)
            push(type);
    }
    if (creature->isLocalPlayer() && player->isInParty() && !(m_playerData && m_playerData->isFighting())) {
        if (player->isPartyLeader()) {
            auto shared = std::make_shared<input::TGameActionSetSharedExperience>(!player->isSharingExperience());
            actions.push_back(shared);
        }
        actions.push_back(input::makeGameAction(EGameAction::LEAVE_PARTY));
    }
    if (!creature->isLocalPlayer() && creature->isPartyMember())
        push(EGameAction::JOIN_AGGRESSION);
}

}
