#pragma once

#include "game/blessingdialogdata.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <cstdint>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameclientMessageBlessingsDialog;
class GameserverMessageBlessingsDialog;
}
}
namespace game {

/// A blessing bit of the protocol (1, 2, 4, … 256); anything else throws.
int blessingFromProtocol(uint32_t Value);
/// A blessing history colour of the protocol (0..6); anything else throws.
int blessingHistoryColorFromProtocol(uint32_t Value);

class TBlessingsProtocolMessageHandler final : public cip::tibia::game::TBlessingsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBlessingsProtocolMessageHandler::TBlessingsProtocolMessageHandler;

public Q_SLOTS:
    void handleBlessingsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& BlessingsDialogMessage);

Q_SIGNALS:
    void sendBlessingsDialog(const ::tibia::protobuf::protocol::GameclientMessageBlessingsDialog& BlessingsDialogMessage);
    void requestBlessingsDialog(const ::tibia::game::TBlessingDialogData& BlessingDialogData);
};

}
}
