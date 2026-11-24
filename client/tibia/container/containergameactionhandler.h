#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QObject>

#include <memory>

namespace tibia::config {
class TClientOptions;
}

namespace tibia::container {

class TContainerStorage;
class TObjectCountStorage;

/// CIP container::TContainerGameActionHandler: the game actions on the
/// inventory, the containers, the supply stash and the depot search. Each
/// becomes one client message, handed to the session through a send* signal;
/// a move onto the supply stash or a stow of a container's content first asks
/// for confirmation when the option stashAskBeforeStowContainerContent is on.
/// The tutorial markers on inventory and container slots go to the container
/// storage.
///
/// Host concern: CipSoft reads the player's object counts from the object
/// instance info storage; qtclient keeps them in the session's object count
/// storage (by type and tier).
class TContainerGameActionHandler final : public cip::tibia::container::TContainerGameActionHandler
{
    Q_OBJECT
public:
    TContainerGameActionHandler(std::shared_ptr<config::TClientOptions> clientOptions, std::shared_ptr<TContainerStorage> containerStorage,
                                std::shared_ptr<TObjectCountStorage> objectCountStorage, QObject* parent = nullptr);
    ~TContainerGameActionHandler() override;
    TContainerGameActionHandler(const TContainerGameActionHandler&) = delete;
    TContainerGameActionHandler& operator=(const TContainerGameActionHandler&) = delete;

Q_SIGNALS:
    // publishGameAction is the generated base's.
    void sendMoveObject(const ::tibia::protobuf::protocol::GameclientMessageMoveObject& MoveObjectMessage);
    void sendEquipObject(const ::tibia::protobuf::protocol::GameclientMessageEquipObject& EquipObjectMessage);
    void sendStashAction(const ::tibia::protobuf::protocol::GameclientMessageStashAction& StashActionMessage);
    void sendOpenDepotSearch(const ::tibia::protobuf::protocol::GameclientMessageOpenDepotSearch& OpenDepotSearchMessage);
    void sendCloseDepotSearch(const ::tibia::protobuf::protocol::GameclientMessageCloseDepotSearch& CloseDepotSearchMessage);
    void sendDepotSearchType(const ::tibia::protobuf::protocol::GameclientMessageDepotSearchType& DepotSearchTypeMessage);
    void sendOpenParentContainer(
        const ::tibia::protobuf::protocol::GameclientMessageOpenParentContainer& OpenParentContainerMessage);
    void sendDepotSearchRetrieve(
        const ::tibia::protobuf::protocol::GameclientMessageDepotSearchRetrieve& DepotSearchRetrieveMessage);

public Q_SLOTS:
    /// Hands an action of a type it handles to its handler; connected to the
    /// router's publishGameActionInternal.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// A move into the inventory or a container (moves onto the map are not
    /// this handler's). A move onto its own place sends nothing.
    void handleMoveObjectGameAction(const std::shared_ptr<const ::tibia::input::TGameActionMoveObject>& pGameActionMoveObject);
    /// Equips the object, or in smart mode the one of the object and its
    /// expire counterpart that is worn, else the one the player has. Nothing
    /// is sent when the player has none of the chosen one.
    void handleTryToEquipGameAction(const std::shared_ptr<const ::tibia::input::TGameActionTryToEquip>& pGameActioTryToEquip);
    void handleRemoveAllTutorialMarkersGameAction();
    /// A marker on an inventory slot or a container slot.
    void handleAddGUITutorialMarkerGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionAddGUITutorialMarker>& pAddGUITutorialMarkerGameAction);
    /// The stash actions take an object in the inventory, a container or the
    /// depot search lists.
    void handleStashStowTypeGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionStashStowType>& pStashStowTypeGameAction);
    void handleStowContainerContentGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionStashStowContainerContent>& pStowContainerContentGameAction);
    void handleStashStowStackGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionStashStowStack>& pStashStowStackGameAction);
    void handleStashRetrieveGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionStashRetrieve>& pStashRetrieveGameAction);
    void handleReqeustReopenStash() override;
    void handleRequestDepotSearchGameAction() override;
    void handleDepotSearchClosedGameAction() override;
    void handleRequestSearchTypeInDepotGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionRequestSearchTypeInDepot>& pRequestSearchTypeInDepotGameAction);
    void handleOpenParentContainerGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionOpenParentContainer>& pOpenParentContainerGameAction);
    void handleDepotSearchRetrieveGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionDepotSearchRetrieve>& pDepotSearchRetrieveGameAction);

private:
    /// CIP +0x10: the client's options.
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<TContainerStorage> m_containerStorage;
    std::shared_ptr<TObjectCountStorage> m_objectCountStorage;
};

} // namespace tibia::container
