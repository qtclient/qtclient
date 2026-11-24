#include "game/playergameactionhandler.h"

#include "creatures/creaturestorage.h"
#include "game/playerdata.h"

#include <stdexcept>
#include <utility>

namespace tibia::game {

using A = input::EGameAction;

namespace {

namespace pb = ::tibia::protobuf::protocol;

/// The PvP mode as the client message's enum: the four modes pass, anything
/// else throws.
uint32_t protocolPvpMode(const int pvpMode)
{
    if (static_cast<unsigned>(pvpMode) < 4)
        return static_cast<uint32_t>(pvpMode);
    throw std::runtime_error("unhandled enum value");
}

/// The outfit request's mode as GetOutfit's enum: CipSoft's static table maps
/// the four modes 0..3 to themselves; anything else throws.
uint32_t protocolOutfitRequestMode(const int mode)
{
    if (static_cast<unsigned>(mode) < 4)
        return static_cast<uint32_t>(mode);
    throw std::runtime_error("unhandled enum value");
}

/// A world map coordinate as the protocol's coordinate (x, y, z).
void setProtocolCoordinate(pb::ProtocolCoordinate* pCoordinate, const Position& position)
{
    pCoordinate->set_x(static_cast<uint32_t>(position.x));
    pCoordinate->set_y(static_cast<uint32_t>(position.y));
    pCoordinate->set_z(static_cast<uint32_t>(position.z));
}

}

TPlayerGameActionHandler::TPlayerGameActionHandler(std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                                   std::shared_ptr<TPlayerData> playerData)
    : m_creatureStorage(std::move(creatureStorage))
    , m_playerData(std::move(playerData))
{
}

TPlayerGameActionHandler::~TPlayerGameActionHandler() = default;

void TPlayerGameActionHandler::handleGameAction(const input::IGameActionPtr pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::ALLOW_ALL_TO_INSPECT_PLAYER:
        allowAllToInspectPlayer();
        return;
    case A::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER:
        withdrawAllowAllToInspectPlayer();
        return;
    case A::MOUNT_PLAYER:
        mountPlayer();
        return;
    case A::UNMOUNT_PLAYER:
        unmountPlayer();
        return;
    case A::TOGGLE_MOUNTED:
        toggleMounted();
        return;
    case A::REQUEST_OUTFITS_FROM_SERVER:
        requestOutfitsFromServer(std::dynamic_pointer_cast<const input::TGameActionRequestOutfitsFromServer>(pGameAction));
        return;
    case A::SET_PLAYER_OUTFIT:
        setPlayerOutfit(std::dynamic_pointer_cast<const input::TGameActionSetPlayerOutfit>(pGameAction));
        return;
    case A::SET_HIRELING_OUTFIT:
        setHirelingOutfit(std::dynamic_pointer_cast<const input::TGameActionSetHirelingOutfit>(pGameAction));
        return;
    case A::SET_HIRELING_NAME:
        setHirelingName(std::dynamic_pointer_cast<const input::TGameActionSetHirelingName>(pGameAction));
        return;
    case A::SET_SHOW_OFF_SOCKET_CONFIGURATION:
        setShowOffSocketConfiguration(std::dynamic_pointer_cast<const input::TGameActionSetShowOffSocketConfiguration>(pGameAction));
        return;
    case A::SET_CREATURE_PODIUM_CONFIGURATION:
        setCreaturePodiumConfiguration(std::dynamic_pointer_cast<const input::TGameActionSetCreaturePodiumConfiguration>(pGameAction));
        return;
    case A::SET_TACTICS:
        setTactics(std::dynamic_pointer_cast<const input::TGameActionSetTactics>(pGameAction));
        return;
    default:
        return;
    }
}

void TPlayerGameActionHandler::mountPlayer()
{
    protobuf::protocol::GameclientMessageMount message;
    message.set_mounted(true);
    Q_EMIT sendMount(message);
}

void TPlayerGameActionHandler::unmountPlayer()
{
    protobuf::protocol::GameclientMessageMount message;
    message.set_mounted(false);
    Q_EMIT sendMount(message);
}

void TPlayerGameActionHandler::toggleMounted()
{
    const std::shared_ptr<creatures::TCreature> player = m_creatureStorage ? m_creatureStorage->player() : nullptr;
    // Host: qtclient's hotkeys can fire before the player creature is known.
    if (!player)
        return;
    if (!player->isMounted())
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionMountPlayer>());
    else
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionUnmountPlayer>());
}

void TPlayerGameActionHandler::requestOutfitsFromServer(
    const std::shared_ptr<const input::TGameActionRequestOutfitsFromServer>& pGameAction)
{
    pb::GameclientMessageGetOutfit message;
    message.set_outfit_type(protocolOutfitRequestMode(pGameAction->mode()));
    message.set_creature_type(pGameAction->typeId());
    message.set_creature_id(pGameAction->creatureId());
    Q_EMIT sendGetOutfit(message);
}

void TPlayerGameActionHandler::setPlayerOutfit(const std::shared_ptr<const input::TGameActionSetPlayerOutfit>& pGameAction)
{
    const Outfit& outfit = pGameAction->outfit();
    pb::GameclientMessageSetOutfit message;
    message.set_outfit_type(0);
    pb::ProtocolOutfitLook* look = message.mutable_outfit();
    look->set_look_type(outfit.getId());
    pb::OutfitColor* colors = look->mutable_colors();
    colors->set_head(outfit.getHead());
    colors->set_body(outfit.getBody());
    colors->set_legs(outfit.getLegs());
    colors->set_feet(outfit.getFeet());
    look->set_addons(outfit.getAddons());
    pb::MountAndSummonOutfitData* mount = message.mutable_mount_and_summon_data();
    mount->set_mount_look_type(outfit.getMount());
    pb::OutfitColor* mountColors = mount->mutable_mount_colors();
    mountColors->set_head(outfit.getMountHead());
    mountColors->set_body(outfit.getMountBody());
    mountColors->set_legs(outfit.getMountLegs());
    mountColors->set_feet(outfit.getMountFeet());
    mount->set_mounted(pGameAction->mountCharacter());
    mount->set_familiar_look_type(outfit.getFamiliar());
    mount->set_random_mount(pGameAction->randomMount());
    Q_EMIT sendSetOutfit(message);
}

void TPlayerGameActionHandler::setHirelingOutfit(const std::shared_ptr<const input::TGameActionSetHirelingOutfit>& pGameAction)
{
    const Outfit& outfit = pGameAction->outfit();
    pb::GameclientMessageSetOutfit message;
    message.set_outfit_type(1);
    pb::ProtocolOutfitLook* look = message.mutable_outfit();
    look->set_look_type(outfit.getId());
    pb::OutfitColor* colors = look->mutable_colors();
    colors->set_head(outfit.getHead());
    colors->set_body(outfit.getBody());
    colors->set_legs(outfit.getLegs());
    colors->set_feet(outfit.getFeet());
    // The first addon in bit 0, the second in bit 1.
    look->set_addons(outfit.getAddons());
    message.set_hireling_id(pGameAction->creatureId());
    Q_EMIT sendSetOutfit(message);
}

void TPlayerGameActionHandler::setCreaturePodiumConfiguration(
    const std::shared_ptr<const input::TGameActionSetCreaturePodiumConfiguration>& pGameAction)
{
    pb::GameclientMessageSetCreaturePodiumConfiguration message;
    message.set_race_id(pGameAction->raceId());
    pb::ShowOffSocketConfiguration* configuration = message.mutable_configuration();
    pb::ShowOffSocketTarget* target = configuration->mutable_target();
    target->set_object_id(pGameAction->podium().itemId);
    target->set_stack_position(pGameAction->podium().stackPosition);
    setProtocolCoordinate(target->mutable_position(), pGameAction->podium().position);
    configuration->set_direction(pGameAction->lookDirection());
    configuration->set_podium_visible(pGameAction->showPodium());
    configuration->set_monster_visible(pGameAction->showCreature());
    Q_EMIT sendSetCreaturePodiumConfiguration(message);
}

void TPlayerGameActionHandler::setHirelingName(const std::shared_ptr<const input::TGameActionSetHirelingName>& pGameAction)
{
    protobuf::protocol::GameclientMessageSetHirelingName message;
    message.set_name(pGameAction->name().toStdString());
    // The action's two ids go out in its order, right after the name.
    message.set_creature_id(static_cast<uint32_t>(pGameAction->value28()));
    message.set_object_id(static_cast<uint32_t>(pGameAction->value2c()));
    Q_EMIT sendSetHirelingName(message);
}

void TPlayerGameActionHandler::setShowOffSocketConfiguration(
    const std::shared_ptr<const input::TGameActionSetShowOffSocketConfiguration>& pGameAction)
{
    const Outfit& outfit = pGameAction->outfit();
    pb::GameclientMessageSetOutfit message;
    message.set_outfit_type(2);
    pb::ProtocolOutfitLook* look = message.mutable_outfit();
    look->set_look_type(outfit.getId());
    pb::OutfitColor* colors = look->mutable_colors();
    colors->set_head(outfit.getHead());
    colors->set_body(outfit.getBody());
    colors->set_legs(outfit.getLegs());
    colors->set_feet(outfit.getFeet());
    look->set_addons(outfit.getAddons());
    pb::ShowOffSocketData* socket = message.mutable_show_off_socket_data();
    socket->set_mount_look_type(outfit.getMount());
    pb::OutfitColor* mountColors = socket->mutable_mount_colors();
    mountColors->set_head(outfit.getMountHead());
    mountColors->set_body(outfit.getMountBody());
    mountColors->set_legs(outfit.getMountLegs());
    mountColors->set_feet(outfit.getMountFeet());
    pb::ShowOffSocketTarget* target = socket->mutable_target();
    setProtocolCoordinate(target->mutable_position(), pGameAction->podium().position);
    target->set_stack_position(pGameAction->podium().stackPosition);
    target->set_object_id(pGameAction->podium().itemId);
    socket->set_direction(pGameAction->lookDirection());
    socket->set_podium_visible(pGameAction->showSocket());
    Q_EMIT sendSetOutfit(message);
}

void TPlayerGameActionHandler::setTactics(const std::shared_ptr<const input::TGameActionSetTactics>& pGameAction)
{
    const input::TGameActionSetTactics::TTactics tactics = pGameAction->tactics();
    m_playerData->setChaseOpponents(tactics.chase);
    m_playerData->setSecureMode(tactics.secure);
    m_playerData->setPvpMode(tactics.pvpMode);
    protobuf::protocol::GameclientMessageSetTactics message;
    message.set_chase_opponent(tactics.chase);
    message.set_secure_mode(tactics.secure);
    message.set_pvp_mode(protocolPvpMode(tactics.pvpMode));
    Q_EMIT sendSetTactics(message);
}

void TPlayerGameActionHandler::allowAllToInspectPlayer()
{
    protobuf::protocol::GameclientMessageInspectPlayer message;
    message.set_tab(6);
    Q_EMIT sendInspectPlayer(message);
}

void TPlayerGameActionHandler::withdrawAllowAllToInspectPlayer()
{
    protobuf::protocol::GameclientMessageInspectPlayer message;
    message.set_tab(7);
    Q_EMIT sendInspectPlayer(message);
}

}
