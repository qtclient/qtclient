#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QObject>

#include <memory>

namespace tibia {
namespace shared {
class TTibiaClipboardHelper;
}
namespace creatures {

class TCreatureStorage;

/// CIP creatures::TCreaturesGameActionHandler: the game actions on one creature.
/// Attack, follow, look at, inspect, the party actions, join aggression and
/// greeting an NPC become client messages on the send* signals (the session
/// puts them into its message queue); the name copy goes to the clipboard.
///
/// qtclient: three of CipSoft's cases are handled by other qtclient handlers
/// (USE_ON_TARGET / USE_ON_YOURSELF by input::TUseWithGameActionHandler,
/// TEMPORARY_SELECT_CREATURE by the battle list storage).
/// CipSoft's slots take the typed action and read its creature id; qtclient's
/// creature actions come in several action classes, so the slots take the
/// action and read the id with input::creatureIdOf.
class TCreaturesGameActionHandler final : public cip::tibia::creatures::TCreaturesGameActionHandler
{
    Q_OBJECT
public:
    TCreaturesGameActionHandler(std::shared_ptr<TCreatureStorage> creatureStorage,
                                std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper);
    ~TCreaturesGameActionHandler() override;
    TCreaturesGameActionHandler(const TCreaturesGameActionHandler&) = delete;
    TCreaturesGameActionHandler& operator=(const TCreaturesGameActionHandler&) = delete;

    /// FOLLOW_CREATURE: the storage's follow target, then Follow with the
    /// creature id as id and sequence.
    virtual void follow(const input::IGameActionPtr& pGameAction);
    /// STOP_FOLLOWING_CREATURE: only while there is a follow target; clears it
    /// and sends Follow 0.
    virtual void stopFollowing();
    /// ATTACK_CREATURE: the storage's attack target, then Attack with the
    /// creature id as id and sequence.
    virtual void attack(const input::IGameActionPtr& pGameAction);
    /// STOP_ATTACKING_CREATURE: only while there is an attack target; clears
    /// it and sends Attack 0.
    virtual void stopAttacking();
    /// COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD: the creature's name.
    virtual void copyCreatureName(const input::IGameActionPtr& pGameAction);
    /// LOOK_AT_CREATURE: LookAtCreature.
    virtual void lookAtCreature(const input::IGameActionPtr& pGameAction);
    /// INVITE_TO_INSPECT_PLAYER .. REVOKE_INSPECT_PLAYER: InspectPlayer, the
    /// tab is the action's offset from 14 (4 for anything else).
    virtual void inspectPlayer(const input::IGameActionPtr& pGameAction);
    virtual void inviteToParty(const input::IGameActionPtr& pGameAction);
    virtual void joinParty(const input::IGameActionPtr& pGameAction);
    virtual void revokePartyInvitation(const input::IGameActionPtr& pGameAction);
    virtual void passPartyLeadership(const input::IGameActionPtr& pGameAction);
    virtual void leaveParty();
    virtual void setSharedExperience(const input::IGameActionPtr& pGameAction);
    /// START_TALKING_TO_NPC: an NPC less than four fields away on the
    /// player's floor gets the default chat channel opened and a Greet; else
    /// the status line says it is too far away.
    virtual void startTalkingToNpc(const input::IGameActionPtr& pGameAction);
    virtual void joinAggression(const input::IGameActionPtr& pGameAction);
    /// ADD_GUI_TUTORIAL_MARKER on a creature located on the world map: the
    /// marker is published again, located at the creature's field.
    virtual void addGuiTutorialMarker(const input::IGameActionPtr& pGameAction);

Q_SIGNALS:
    void sendAttack(tibia::protobuf::protocol::GameclientMessageAttack AttackMessage);
    void sendFollow(tibia::protobuf::protocol::GameclientMessageFollow FollowMessage);
    void sendLookAtCreature(tibia::protobuf::protocol::GameclientMessageLookAtCreature LookAtCreatureMessage);
    void sendInspectPlayer(tibia::protobuf::protocol::GameclientMessageInspectPlayer InspectPlayerMessage);
    void sendInviteToParty(tibia::protobuf::protocol::GameclientMessageInviteToParty PartyMessage);
    void sendJoinParty(tibia::protobuf::protocol::GameclientMessageJoinParty PartyMessage);
    void sendRevokeInvitation(tibia::protobuf::protocol::GameclientMessageRevokeInvitation PartyMessage);
    void sendPassLeadership(tibia::protobuf::protocol::GameclientMessagePassLeadership PartyMessage);
    void sendLeaveParty(tibia::protobuf::protocol::GameclientMessageLeaveParty PartyMessage);
    void sendShareExperience(tibia::protobuf::protocol::GameclientMessageShareExperience ShareExperienceMessage);
    void sendGreet(tibia::protobuf::protocol::GameclientMessageGreet GreetMessage);
    void sendJoinAggression(tibia::protobuf::protocol::GameclientMessageJoinAggression JoinAggressionMessage);

public Q_SLOTS:
    /// Connected to the router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    std::shared_ptr<TCreatureStorage> m_creatureStorage;
    std::shared_ptr<shared::TTibiaClipboardHelper> m_clipboardHelper;
};

}
}
