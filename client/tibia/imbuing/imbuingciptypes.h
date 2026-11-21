#pragma once

// CIP-named imbuing surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageCloseImbuingDialog;
class GameserverMessageDead;
class GameserverMessageImbuementDurations;
class GameserverMessageImbuingDialogRefresh;
class GameserverMessageShowMessageDialog;
}

namespace tibia {
namespace imbuing {

class TImbuingStorage;

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TImbuementDurationsStorage final : public cip::tibia::imbuing::TImbuementDurationsStorage
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuementDurationsStorage::TImbuementDurationsStorage;
};

/// The imbuing messages of a game session: they fill its imbuing storage
/// and ask the imbuing dialog to show, refresh or close.
class TImbuingProtocolMessageHandler final : public cip::tibia::imbuing::TImbuingProtocolMessageHandler
{
    Q_OBJECT
public:
    TImbuingProtocolMessageHandler(TImbuingStorage* imbuingStorage, TImbuementDurationsStorage* imbuementDurationsStorage,
                                   QObject* parent = nullptr);

    /// The whole refresh goes to the storage; the dialog then shows or updates.
    void handleImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& message);
    /// Only the dialog types that report an imbuing result (mask 0xc0f).
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    void handleCloseImbuingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseImbuingDialog& CloseImbuingDialogMessage);
    /// Dying closes the dialog.
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& DeadMessage);
    void handleImbuementDurationsMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuementDurations& ImbuementDurationsMessage);

private:
    TImbuingStorage* m_imbuingStorage;
    TImbuementDurationsStorage* m_imbuementDurationsStorage;
};

}
}

