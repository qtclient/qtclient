#pragma once

// CIP-named imbuing surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageCloseImbuingDialog;
class GameserverMessageImbuementDurations;
class GameserverMessageImbuingDialogRefresh;
class GameserverMessageShowMessageDialog;
}

namespace tibia {
namespace imbuing {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TImbuementDurationsStorage final : public cip::tibia::imbuing::TImbuementDurationsStorage
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuementDurationsStorage::TImbuementDurationsStorage;
};

class TImbuingProtocolMessageHandler final : public cip::tibia::imbuing::TImbuingProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuingProtocolMessageHandler::TImbuingProtocolMessageHandler;
    /// The whole refresh goes to the storage; the dialog then shows or updates.
    static void handleImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& message);
    /// Only the dialog types that report an imbuing result (mask 0xc0f).
    static void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    static void handleCloseImbuingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseImbuingDialog& CloseImbuingDialogMessage);
    static void handleImbuementDurationsMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuementDurations& ImbuementDurationsMessage);
};

}
}

