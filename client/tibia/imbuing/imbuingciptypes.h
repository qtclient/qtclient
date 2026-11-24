#pragma once

// CIP-named imbuing surfaces (promoted from cipwire).

#include "container/containerciptypes.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageCloseImbuingDialog;
class GameserverMessageDead;
class GameserverMessageImbuementDurations;
class GameserverMessageImbuingDialogRefresh;
class GameserverMessageShowMessageDialog;
}

namespace tibia::protocol {
class TGameActionContextHelper;
}
namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace imbuing {

class TImbuingStorage;


/// One imbuement slot of a tracked object.
struct TImbuementSlotInformation
{
    /// The slot holds an imbuement (the rest is empty otherwise).
    bool imbued = false;
    QString name;
    uint32_t iconId = 0;
    /// Time left in milliseconds.
    qint64 remainingMilliseconds = 0;
    /// Its time runs down (the object is worn).
    bool decaying = false;
};

/// One object of the imbuement tracker: where it is worn, the object and its slots.
struct TImbuedObjectInformation
{
    /// The inventory slot (1..13); 99 until set.
    uint32_t inventorySlot = 99;
    /// Host: the object as an inventory slot (CipSoft keeps an object instance of the session's object storage).
    container::TInventorySlot object;
    std::vector<TImbuementSlotInformation> slots;
};

/// The imbued objects the server reported last (the imbuement tracker's rows).
class TImbuementDurationsStorage final : public cip::tibia::imbuing::TImbuementDurationsStorage
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuementDurationsStorage::TImbuementDurationsStorage;

    const std::vector<TImbuedObjectInformation>& imbuedObjects() const { return m_imbuedObjects; }
    /// Replaces the objects and always emits imbuedObjectsChanged.
    void setImbuedObjects(const std::vector<TImbuedObjectInformation>& ImbuedObjects);

private:
    std::vector<TImbuedObjectInformation> m_imbuedObjects;
};

/// The imbuing messages of a game session: they fill its imbuing storage
/// and ask the imbuing dialog to show, refresh or close.
class TImbuingProtocolMessageHandler final : public cip::tibia::imbuing::TImbuingProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler keeps the session's imbuing storages, its game action
    /// context helper and its clock. Host: the appearance instance factory is
    /// static in qtclient.
    TImbuingProtocolMessageHandler(std::shared_ptr<TImbuingStorage> imbuingStorage,
                                   std::shared_ptr<protocol::TGameActionContextHelper> gameActionContextHelper,
                                   std::shared_ptr<TImbuementDurationsStorage> imbuementDurationsStorage,
                                   std::shared_ptr<shared::IClockService> clockService);

    /// The whole refresh goes to the storage; the dialog then shows or updates.
    void handleImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& message);
    /// Only the dialog types that report an imbuing result (mask 0xc0f).
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    void handleCloseImbuingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseImbuingDialog& CloseImbuingDialogMessage);
    /// Dying closes the dialog.
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& DeadMessage);
    void handleImbuementDurationsMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuementDurations& ImbuementDurationsMessage);

Q_SIGNALS:
    void sendPickObjectToImbue(const ::tibia::protobuf::protocol::GameclientMessagePickObjectToImbue& PickObjectToImbueMessage);
    void sendApplyImbuement(const ::tibia::protobuf::protocol::GameclientMessageApplyImbuement& ApplyImbuementMessage);
    void sendApplyClearingCharm(const ::tibia::protobuf::protocol::GameclientMessageApplyClearingCharm& ApplyClearingCharmMessage);
    void sendCloseImbuingDialog(const ::tibia::protobuf::protocol::GameclientMessageCloseImbuingDialog& CloseImbuingDialogMessage);
    void sendTrackImbuementsMessage(const ::tibia::protobuf::protocol::GameclientMessageTrackImbuements& TrackImbuementsMessage);

private:
    std::shared_ptr<TImbuingStorage> m_imbuingStorage;
    std::shared_ptr<protocol::TGameActionContextHelper> m_gameActionContextHelper;
    std::shared_ptr<TImbuementDurationsStorage> m_imbuementDurationsStorage;
    std::shared_ptr<shared::IClockService> m_clockService;
};

}
}

