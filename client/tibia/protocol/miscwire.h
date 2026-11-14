#pragma once

// Classic wire → protobuf for the small single-purpose server messages
// (store indicators, prey timers, trackers, news, world time, death, balances).

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia::protocol {

/// Fill one of the messages isMiscMessage accepts; 0 when the body is short.
size_t fillMiscMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isMiscMessage(int opcode);

}
