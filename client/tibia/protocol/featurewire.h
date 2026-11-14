#pragma once

// Classic wire → protobuf for the feature-panel server messages
// (trackers, analysers, quick loot, task board, spells, dialogs, balances).

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia::protocol {

/// Fill one of the messages isFeatureMessage accepts; 0 when the body is short.
size_t fillFeatureMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isFeatureMessage(int opcode);

}
