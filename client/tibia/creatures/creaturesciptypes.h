#pragma once

// CIP-named creatures surfaces (promoted from cipwire).

#include "creatures/creaturestorage.h"
#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace exivaoptions {
class TExivaOptionsStorage;
}
namespace creatures {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBattleListGameActionProvider final : public ::tibia::CipTyped {};

class TBattleListRenderProvider final : public cip::tibia::creatures::TBattleListRenderProvider
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TBattleListRenderProvider::TBattleListRenderProvider;
};

/// CipSoft motion types: composed on each `TCreature` (see creaturemovement*.h).

/// CIP `creatures::TCreatureProtocolMessageHandler` — qtclient Q→H for filled creature
/// attribute opcodes + MoveCreature (109). CIP contract has queue
/// `receivedCreature*Message` / `receivedMoveCreatureMessage` only (no
/// `handleCreature*Message` / `handleMoveCreatureMessage` moc slots) — each
/// message applies its fields to the stored creature (`applyProtocolCreature`
/// for a whole creature) or `Map::applyCreatureMove` in
/// `onServerMessageEnqueued`; do not invent handle* names.
class TCreatureProtocolMessageHandler final : public cip::tibia::creatures::TCreatureProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TCreatureProtocolMessageHandler(QObject* parent = nullptr);

    static TCreatureProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TCreatureProtocolMessageHandler* handler);

    /// Tip Q→H wire: peel structured creature-attribute opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode);

private:
    /// A creature mark: timed or static square, or the attack animation.
    void handleCreatureMarksMessage(uint32_t creatureId, uint32_t kind, uint32_t value);
    /// The server changed the attack or follow target the sequence names.
    void handleUpdateTargetMessage(uint32_t sequence, uint32_t target);

    /// Option gameWindowShowAttackAnimation.
    bool m_showAttackAnimation{ true };
};

class TCreaturesGameActionHandler final : public cip::tibia::creatures::TCreaturesGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TCreaturesGameActionHandler::TCreaturesGameActionHandler;
};

class TInspectPlayerGameActionProvider final : public ::tibia::CipTyped {};

class TPartyGameActionProvider final : public ::tibia::CipTyped {};

class TUnjustifiedPointsProtocolMessageHandler final : public cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler::TUnjustifiedPointsProtocolMessageHandler;
public:
    /// Tip Q→H wire: PvpSituations from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode);
};

class TUnjustifiedPointsStorage final : public cip::tibia::creatures::TUnjustifiedPointsStorage
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TUnjustifiedPointsStorage::TUnjustifiedPointsStorage;
};

class TVipStorage;

/// The VIP list's game actions: sends the buddy messages and keeps the
/// storage in step (CIP: creatures::TVipGameActionHandler).
class TVipGameActionHandler final : public cip::tibia::creatures::TVipGameActionHandler
{
    Q_OBJECT
public:
    TVipGameActionHandler(TVipStorage* storage, QObject* parent = nullptr);
    ~TVipGameActionHandler() override;

    void handleGameAction(const ::tibia::input::TGameAction& action);
    void handleAddNewVipGameAction(const ::tibia::input::TGameAction& action);
    /// Removes the VIP named by the action, locally and on the server.
    void handleRemoveVipGameAction(const ::tibia::input::TGameAction& action);
    /// Stores the edited VIP (keeping its state) and sends the edit.
    void handleRequestVipDataUpdateGameAction(const ::tibia::input::TGameAction& action);
    void handleAddNewVipGroupGameAction(const ::tibia::input::TGameAction& action);
    void handleRemoveVipGroupGameAction(const ::tibia::input::TGameAction& action);
    void handleRequestVipGroupUpdateGameAction(const ::tibia::input::TGameAction& action);

private:
    TVipStorage* m_storage;
};

/// BuddyData, BuddyStatusChange and BuddyGroupData into the VIP storage
/// (CIP: creatures::TVipProtocolMessageHandler).
class TVipProtocolMessageHandler final : public cip::tibia::creatures::TVipProtocolMessageHandler
{
    Q_OBJECT
public:
    TVipProtocolMessageHandler(TVipStorage* storage, QObject* parent = nullptr);

    /// Tip Q→H wire: the buddy messages from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode);

    void handleBuddyData(const ::tibia::protobuf::protocol::GameserverMessageBuddyData& message);
    void handleBuddyStatusChange(const ::tibia::protobuf::protocol::GameserverMessageBuddyStatusChange& message);
    void handleBuddyGroupData(const ::tibia::protobuf::protocol::GameserverMessageBuddyGroupData& message);

private:
    TVipStorage* m_storage;
};

/// The game actions of a click in the VIP widget (CIP:
/// creatures::TVipWidgetGameActionProvider).
class TVipWidgetGameActionProvider final
{
public:
    TVipWidgetGameActionProvider(TVipStorage* storage, ::tibia::exivaoptions::TExivaOptionsStorage* exivaOptions);

    /// The widget's own actions: context menu, add VIP, sorting, and the
    /// offline / groups toggles for the current filter.
    ::tibia::input::TGameActionList widgetGameActions(bool hideOffline, bool groupsHidden) const;
    /// The actions of a click on `vipName` / `groupName` (empty for none).
    ::tibia::input::TGameActionList gameActions(const QString& vipName, const QString& groupName, bool hideOffline,
                                                bool groupsHidden) const;
    /// Premium players' group actions: add, and remove / edit an editable group.
    ::tibia::input::TGameActionList groupGameActions(const QString& groupName) const;
    /// The session's EWorldType (exiva whitelist entries on optional PvP, 1).
    void setWorldType(int worldType);

private:
    TVipStorage* m_storage;
    ::tibia::exivaoptions::TExivaOptionsStorage* m_exivaOptions;
    int m_worldType = -1;
};

}
}
