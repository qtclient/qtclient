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
/// mounting, the outfit window (request and the player's, a hireling's and a
/// podium's outfit), the hireling's name, the creature podium, the tactics and
/// the allow-all inspection switch. Each becomes the client message the
/// session's queue sends.
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
    /// REQUEST_OUTFITS_FROM_SERVER: GetOutfit with the action's mode, object type
    /// and creature.
    virtual void requestOutfitsFromServer(const std::shared_ptr<const input::TGameActionRequestOutfitsFromServer>& pGameAction);
    /// SET_PLAYER_OUTFIT: SetOutfit of type 0, the outfit and the mount and
    /// summon data (mount look and colours, familiar, random mount, ride).
    virtual void setPlayerOutfit(const std::shared_ptr<const input::TGameActionSetPlayerOutfit>& pGameAction);
    /// SET_HIRELING_OUTFIT: SetOutfit of type 1, the outfit and the hireling.
    virtual void setHirelingOutfit(const std::shared_ptr<const input::TGameActionSetHirelingOutfit>& pGameAction);
    /// SET_CREATURE_PODIUM_CONFIGURATION: SetCreaturePodiumConfiguration with the
    /// race and the podium's configuration.
    virtual void setCreaturePodiumConfiguration(
        const std::shared_ptr<const input::TGameActionSetCreaturePodiumConfiguration>& pGameAction);
    /// SET_HIRELING_NAME: SetHirelingName with the name and the action's two ids,
    /// in the action's order.
    virtual void setHirelingName(const std::shared_ptr<const input::TGameActionSetHirelingName>& pGameAction);
    /// SET_SHOW_OFF_SOCKET_CONFIGURATION: SetOutfit of type 2, the outfit and the
    /// show-off socket data (podium, mount look and colours, direction, podium shown).
    virtual void setShowOffSocketConfiguration(
        const std::shared_ptr<const input::TGameActionSetShowOffSocketConfiguration>& pGameAction);
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
    void sendGetOutfit(tibia::protobuf::protocol::GameclientMessageGetOutfit GetOutfitMessage);
    void sendSetOutfit(tibia::protobuf::protocol::GameclientMessageSetOutfit SetOutfitMessage);
    void sendSetCreaturePodiumConfiguration(
        tibia::protobuf::protocol::GameclientMessageSetCreaturePodiumConfiguration SetCreaturePodiumConfigurationMessage);
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
