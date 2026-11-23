#include "game/playergameactionhandler.h"

#include "creatures/creaturestorage.h"
#include "game/playerdata.h"

#include <stdexcept>
#include <utility>

namespace tibia::game {

using A = input::EGameAction;

namespace {

/// The PvP mode as the client message's enum: the four modes pass, anything
/// else throws.
uint32_t protocolPvpMode(const int pvpMode)
{
    if (static_cast<unsigned>(pvpMode) < 4)
        return static_cast<uint32_t>(pvpMode);
    throw std::runtime_error("unhandled enum value");
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
    case A::SET_HIRELING_NAME:
        setHirelingName(std::dynamic_pointer_cast<const input::TGameActionSetHirelingName>(pGameAction));
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

void TPlayerGameActionHandler::setHirelingName(const std::shared_ptr<const input::TGameActionSetHirelingName>& pGameAction)
{
    protobuf::protocol::GameclientMessageSetHirelingName message;
    message.set_name(pGameAction->name().toStdString());
    // The action's two ids go out in its order, right after the name.
    message.set_creature_id(static_cast<uint32_t>(pGameAction->value28()));
    message.set_object_id(static_cast<uint32_t>(pGameAction->value2c()));
    Q_EMIT sendSetHirelingName(message);
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
