#pragma once

// CIP-named creatures surfaces (promoted from cipwire).

#include "input/gameactiondispatch.h"
#include "creatures/creaturestorage.h"
#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QByteArray>
#include <QObject>

#include <cstdint>
#include <memory>

namespace tibia {
namespace exivaoptions {
class TExivaOptionsStorage;
}
namespace worldmap {
class TWorldMapStorage;
}
namespace creatures {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);



/// CipSoft motion types: composed on each `TCreature` (see creaturemovement*.h).

/// Applies the creature messages to the session's creature storage: moves,
/// whole creatures and single attributes, marks and the player's id. One per
/// game session; creatures move on the session's world map.
class TCreatureProtocolMessageHandler final : public cip::tibia::creatures::TCreatureProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TCreatureProtocolMessageHandler(worldmap::TWorldMapStorage* worldMapStorage, QObject* parent = nullptr);

    void handleMoveCreatureMessage(const ::tibia::protobuf::protocol::GameserverMessageMoveCreature& message);
    void handleLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& message);
    void handleSessionDumpStartMessage(const ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart& message);
    void handleCreatureUpdateMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureUpdate& message);
    void handleCreatureOutfitMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureOutfit& message);
    void handleCreatureHealthMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureHealth& message);
    void handleCreatureSpeedMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed& message);
    void handleCreatureSkullMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSkull& message);
    void handleCreaturePartyMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureParty& message);
    void handleInspectionStateMessage(const ::tibia::protobuf::protocol::GameserverMessageInspectionState& message);
    void handleCreatureUnpassMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureUnpass& message);
    void handleCreatureMarksMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureMarks& message);
    void handleCreatureDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureData& message);
    void handleCreatureLightMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureLight& message);
    void handleUpdateTargetMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateTarget& message);
    /// Product server: a creature's type and master.
    void handleCreatureTypeMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureType& message);

private:
    /// A creature mark: timed or static square, or the attack animation.
    void applyCreatureMarks(uint32_t creatureId, uint32_t kind, uint32_t value);
    /// The server changed the attack or follow target the sequence names.
    void applyUpdateTarget(uint32_t sequence, uint32_t target);

    /// The session's world map.
    worldmap::TWorldMapStorage* m_worldMapStorage = nullptr;
    /// Option gameWindowShowAttackAnimation.
    bool m_showAttackAnimation{ true };
};

/// CIP creatures::TUnjustifiedPointsStorage: the player's unjustified kill
/// progress (per day, week and month: the percentage towards the next skull and
/// the kills left), the skull's remaining duration and the open PvP situations.
/// Every change emits unjustifiedPointsDataChanged.
class TUnjustifiedPointsStorage final : public cip::tibia::creatures::TUnjustifiedPointsStorage
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TUnjustifiedPointsStorage::TUnjustifiedPointsStorage;

    uint8_t progressDay() const { return m_progressDay; }
    uint8_t fullKillsToNextSkullDay() const { return m_fullKillsToNextSkullDay; }
    uint8_t progressWeek() const { return m_progressWeek; }
    uint8_t fullKillsToNextSkullWeek() const { return m_fullKillsToNextSkullWeek; }
    uint8_t progressMonth() const { return m_progressMonth; }
    uint8_t fullKillsToNextSkullMonth() const { return m_fullKillsToNextSkullMonth; }
    uint8_t skullDuration() const { return m_skullDuration; }
    uint8_t openPvpSituations() const { return m_openPvpSituations; }

    void setUnjustifiedPoints(uint8_t progressDay, uint8_t fullKillsToNextSkullDay, uint8_t progressWeek,
                              uint8_t fullKillsToNextSkullWeek, uint8_t progressMonth, uint8_t fullKillsToNextSkullMonth,
                              uint8_t skullDuration);
    void setOpenPvpSituations(uint8_t openPvpSituations);

private:
    uint8_t m_progressDay = 0;
    uint8_t m_fullKillsToNextSkullDay = 0;
    uint8_t m_progressWeek = 0;
    uint8_t m_fullKillsToNextSkullWeek = 0;
    uint8_t m_progressMonth = 0;
    uint8_t m_fullKillsToNextSkullMonth = 0;
    uint8_t m_skullDuration = 0;
    uint8_t m_openPvpSituations = 0;
};

/// CIP creatures::TUnjustifiedPointsProtocolMessageHandler: the server's
/// unjustified points and PvP situations go into the session's storage.
class TUnjustifiedPointsProtocolMessageHandler final : public cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TUnjustifiedPointsProtocolMessageHandler(std::shared_ptr<TUnjustifiedPointsStorage> unjustifiedPointsStorage);

public Q_SLOTS:
    void handleUnjustifiedPoints(const ::tibia::protobuf::protocol::GameserverMessageUnjustifiedPoints& UnjustifiedPointsMessage);
    void handlePvpSituations(const ::tibia::protobuf::protocol::GameserverMessagePvpSituations& PvpSituationsMessage);

private:
    std::shared_ptr<TUnjustifiedPointsStorage> m_unjustifiedPointsStorage;
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

    void handleGameAction(const ::tibia::input::IGameAction& action);
    void handleAddNewVipGameAction(const ::tibia::input::TGameActionVip& action);
    /// Removes the VIP named by the action, locally and on the server.
    void handleRemoveVipGameAction(const ::tibia::input::TGameActionVip& action);
    /// Stores the edited VIP (keeping its state) and sends the edit.
    void handleRequestVipDataUpdateGameAction(const ::tibia::input::TGameActionRequestVipDataUpdate& action);
    void handleAddNewVipGroupGameAction(const ::tibia::input::TGameActionVipGroup& action);
    void handleRemoveVipGroupGameAction(const ::tibia::input::TGameActionVipGroup& action);
    void handleRequestVipGroupUpdateGameAction(const ::tibia::input::TGameActionRequestVipGroupUpdate& action);

Q_SIGNALS:
    void sendAddBuddy(const ::tibia::protobuf::protocol::GameclientMessageAddBuddy& AddBuddyMessage);
    void sendRemoveBuddy(const ::tibia::protobuf::protocol::GameclientMessageRemoveBuddy& RemoveBuddyMessage);
    void sendEditBuddy(const ::tibia::protobuf::protocol::GameclientMessageEditBuddy& EditBuddyMessage);
    void sendBuddyGroup(const ::tibia::protobuf::protocol::GameclientMessageBuddyGroup& BuddyGroupMessage);

private:
    TVipStorage* m_storage;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

/// BuddyData, BuddyStatusChange and BuddyGroupData into the VIP storage
/// (CIP: creatures::TVipProtocolMessageHandler).
class TVipProtocolMessageHandler final : public cip::tibia::creatures::TVipProtocolMessageHandler
{
    Q_OBJECT
public:
    TVipProtocolMessageHandler(TVipStorage* storage, QObject* parent = nullptr);

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
