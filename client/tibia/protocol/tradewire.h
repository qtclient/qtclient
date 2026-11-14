#pragma once

// Classic wire → protobuf for NPC and player trade.

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia::protocol {

/// Fill a trade message (NPCOffer, PlayerGoods, CloseNPCTrade, OwnOffer,
/// CounterOffer, CloseTrade); 0 when the body is short (and for the empty
/// close messages, which measure 0).
size_t fillTradeMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isTradeMessage(int opcode);

}
