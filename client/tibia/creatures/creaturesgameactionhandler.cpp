#include "creatures/creaturesgameactionhandler.h"

#include "appearances/creatureplaceholderappearanceinstance.h"
#include "chat/chatchannel.h"
#include "creatures/creaturestorage.h"
#include "shared/tibiaclipboardhelper.h"

#include <QCoreApplication>

#include <cstdlib>
#include <utility>

namespace tibia::creatures {

using A = input::EGameAction;

TCreaturesGameActionHandler::TCreaturesGameActionHandler(std::shared_ptr<TCreatureStorage> creatureStorage,
                                                         std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper)
    : m_creatureStorage(std::move(creatureStorage))
    , m_clipboardHelper(std::move(clipboardHelper))
{
}

TCreaturesGameActionHandler::~TCreaturesGameActionHandler() = default;

void TCreaturesGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::LOOK_AT_CREATURE:
        lookAtCreature(pGameAction);
        break;
    case A::INVITE_TO_INSPECT_PLAYER:
    case A::ASK_TO_INSPECT_PLAYER:
    case A::ALLOW_INSPECT_PLAYER:
    case A::INSPECT_PLAYER:
    case A::REVOKE_INSPECT_PLAYER:
        inspectPlayer(pGameAction);
        break;
    case A::ATTACK_CREATURE:
        attack(pGameAction);
        break;
    case A::STOP_ATTACKING_CREATURE:
        stopAttacking();
        break;
    case A::FOLLOW_CREATURE:
        follow(pGameAction);
        break;
    case A::STOP_ATTACKING_FOLLOWING_WALKING:
        stopAttacking();
        stopFollowing();
        break;
    case A::STOP_FOLLOWING_CREATURE:
        stopFollowing();
        break;
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
        copyCreatureName(pGameAction);
        break;
    case A::START_TALKING_TO_NPC:
        startTalkingToNpc(pGameAction);
        break;
    case A::JOIN_AGGRESSION:
        joinAggression(pGameAction);
        break;
    case A::INVITE_TO_PARTY:
        inviteToParty(pGameAction);
        break;
    case A::JOIN_PARTY:
        joinParty(pGameAction);
        break;
    case A::REVOKE_PARTY_INVITATION:
        revokePartyInvitation(pGameAction);
        break;
    case A::PASS_PARTY_LEADERSHIP:
        passPartyLeadership(pGameAction);
        break;
    case A::LEAVE_PARTY:
        leaveParty();
        break;
    case A::SET_SHARED_EXPERIENCE:
        setSharedExperience(pGameAction);
        break;
    case A::ADD_GUI_TUTORIAL_MARKER:
        addGuiTutorialMarker(pGameAction);
        break;
    default:
        break;
    }
}

void TCreaturesGameActionHandler::follow(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageFollow message;
    const uint32_t creatureId = input::creatureIdOf(*pGameAction);
    m_creatureStorage->setFollowID(creatureId);
    message.set_creature_id(creatureId);
    message.set_sequence(creatureId);
    Q_EMIT sendFollow(message);
}

void TCreaturesGameActionHandler::stopFollowing()
{
    if (m_creatureStorage->followID() == 0)
        return;
    protobuf::protocol::GameclientMessageFollow message;
    m_creatureStorage->setFollowID(0);
    message.set_creature_id(0);
    message.set_sequence(0);
    Q_EMIT sendFollow(message);
}

void TCreaturesGameActionHandler::attack(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageAttack message;
    const uint32_t creatureId = input::creatureIdOf(*pGameAction);
    m_creatureStorage->setAttackID(creatureId);
    message.set_creature_id(creatureId);
    message.set_sequence(creatureId);
    Q_EMIT sendAttack(message);
}

void TCreaturesGameActionHandler::stopAttacking()
{
    if (m_creatureStorage->attackID() == 0)
        return;
    protobuf::protocol::GameclientMessageAttack message;
    m_creatureStorage->setAttackID(0);
    message.set_creature_id(0);
    message.set_sequence(0);
    Q_EMIT sendAttack(message);
}

void TCreaturesGameActionHandler::copyCreatureName(const input::IGameActionPtr& pGameAction)
{
    const std::shared_ptr<TCreature> creature = m_creatureStorage->creatureAt(input::creatureIdOf(*pGameAction));
    // qtclient: CipSoft reads the name without checking that the creature is known.
    if (!creature)
        return;
    m_clipboardHelper->setTextInClipboard(creature->name());
}

void TCreaturesGameActionHandler::lookAtCreature(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageLookAtCreature message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendLookAtCreature(message);
}

void TCreaturesGameActionHandler::inspectPlayer(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageInspectPlayer message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    const int type = static_cast<int>(pGameAction->gameActionType());
    const int first = static_cast<int>(A::INVITE_TO_INSPECT_PLAYER);
    // INVITE_TO_INSPECT_PLAYER (15) .. REVOKE_INSPECT_PLAYER (19) are tabs 1 .. 5.
    message.set_tab(static_cast<unsigned>(type - first) < 5u ? static_cast<uint32_t>(type - (first - 1)) : 4u);
    Q_EMIT sendInspectPlayer(message);
}

void TCreaturesGameActionHandler::inviteToParty(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageInviteToParty message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendInviteToParty(message);
}

void TCreaturesGameActionHandler::joinParty(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageJoinParty message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendJoinParty(message);
}

void TCreaturesGameActionHandler::revokePartyInvitation(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageRevokeInvitation message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendRevokeInvitation(message);
}

void TCreaturesGameActionHandler::passPartyLeadership(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessagePassLeadership message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendPassLeadership(message);
}

void TCreaturesGameActionHandler::leaveParty()
{
    Q_EMIT sendLeaveParty(protobuf::protocol::GameclientMessageLeaveParty());
}

void TCreaturesGameActionHandler::setSharedExperience(const input::IGameActionPtr& pGameAction)
{
    const auto* shared = input::gameActionAs<input::TGameActionSetSharedExperience>(pGameAction);
    if (!shared)
        return;
    protobuf::protocol::GameclientMessageShareExperience message;
    message.set_active(shared->enable());
    Q_EMIT sendShareExperience(message);
}

void TCreaturesGameActionHandler::startTalkingToNpc(const input::IGameActionPtr& pGameAction)
{
    const std::shared_ptr<TCreature> player = m_creatureStorage->player();
    // qtclient: CipSoft reads the player's position without checking for a player.
    if (!player)
        return;
    const Position from = player->position();
    const uint32_t creatureId = input::creatureIdOf(*pGameAction);
    const std::shared_ptr<TCreature> npc = m_creatureStorage->creatureAt(creatureId);
    if (!npc)
        return;
    const Position& to = npc->position();
    if (std::abs(int(to.x) - int(from.x)) < 4 && std::abs(int(to.y) - int(from.y)) < 4 && to.z == from.z) {
        // The default channel (local chat) opens, then the NPC is greeted.
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
            A::OPEN_CHAT_CHANNEL, std::make_shared<const chat::TChatChannelIdentifier>()));
        protobuf::protocol::GameclientMessageGreet message;
        message.set_creature_id(creatureId);
        Q_EMIT sendGreet(message);
        return;
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(qtTrId("you_are_too_far_away")));
}

void TCreaturesGameActionHandler::joinAggression(const input::IGameActionPtr& pGameAction)
{
    protobuf::protocol::GameclientMessageJoinAggression message;
    message.set_creature_id(input::creatureIdOf(*pGameAction));
    Q_EMIT sendJoinAggression(message);
}

void TCreaturesGameActionHandler::addGuiTutorialMarker(const input::IGameActionPtr& pGameAction)
{
    // A marker on a creature somewhere on the world map is published again,
    // located at the field of the creature's placeholder.
    const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(pGameAction);
    if (!marker || !marker->location()
        || marker->location()->gameActionContextType() != input::EGameActionContextType::LOCATION_WORLD_MAP
        || !marker->object()
        || marker->object()->gameActionContextType() != input::EGameActionContextType::OBJECT_CREATURE)
        return;
    const std::shared_ptr<TCreature> creature = m_creatureStorage->creatureAt(input::creatureIdOf(marker->object().get()));
    if (!creature)
        return;
    // qtclient: CipSoft uses the placeholder and its creature without checking them.
    const std::shared_ptr<appearances::TCreaturePlaceholderAppearanceInstance>& placeholder =
        creature->placeholderAppearance();
    const std::shared_ptr<TCreature> placed = placeholder ? placeholder->creature() : nullptr;
    if (!placed)
        return;
    // qtclient: the actions are immutable, so the marker is published as a copy
    // with the new location (CipSoft sets the location on the action itself).
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionAddGUITutorialMarker>(
        std::make_shared<const input::TLocationWorldMapCoordinateContext>(placed->position()), marker->object(),
        marker->markerId()));
}

}
