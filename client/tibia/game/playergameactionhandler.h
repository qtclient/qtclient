#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QObject>

#include <memory>

namespace tibia {
namespace creatures {
class TCreatureStorage;
}
}

namespace tibia {
namespace game {

class TPlayerData;

/// The game actions on the player's own character (CIP game::TPlayerGameActionHandler):
/// mounting, the hireling's name, the tactics and the allow-all inspection switch.
/// Each becomes the client message the session's queue sends.
class TPlayerGameActionHandler final : public cip::tibia::game::TPlayerGameActionHandler
{
    Q_OBJECT
public:
    TPlayerGameActionHandler(std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                             std::shared_ptr<TPlayerData> playerData);
    ~TPlayerGameActionHandler() override;
    TPlayerGameActionHandler(const TPlayerGameActionHandler&) = delete;
    TPlayerGameActionHandler& operator=(const TPlayerGameActionHandler&) = delete;

    /// MOUNT_PLAYER: Mount with mounted set.
    virtual void mountPlayer();
    /// UNMOUNT_PLAYER: Mount with mounted cleared.
    virtual void unmountPlayer();
    /// TOGGLE_MOUNTED: publishes UNMOUNT_PLAYER while the player is mounted,
    /// else MOUNT_PLAYER.
    virtual void toggleMounted();
    /// SET_HIRELING_NAME: SetHirelingName with the name and the action's two ids,
    /// in the action's order.
    virtual void setHirelingName(const std::shared_ptr<const input::TGameActionSetHirelingName>& pGameAction);
    /// SET_TACTICS: the player data takes the action's chase, secure and PvP
    /// mode, then SetTactics sends them.
    virtual void setTactics(const std::shared_ptr<const input::TGameActionSetTactics>& pGameAction);
    /// ALLOW_ALL_TO_INSPECT_PLAYER: InspectPlayer of type 6.
    virtual void allowAllToInspectPlayer();
    /// WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER: InspectPlayer of type 7.
    virtual void withdrawAllowAllToInspectPlayer();

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);
    void sendMount(tibia::protobuf::protocol::GameclientMessageMount MountMessage);
    void sendSetHirelingName(tibia::protobuf::protocol::GameclientMessageSetHirelingName SetHirelingNameMessage);
    void sendInspectPlayer(tibia::protobuf::protocol::GameclientMessageInspectPlayer InspectPlayerMessage);
    void sendSetTactics(tibia::protobuf::protocol::GameclientMessageSetTactics SetTacticsMessage);

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<TPlayerData> m_playerData;
};

}
}
