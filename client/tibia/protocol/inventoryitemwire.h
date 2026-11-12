#pragma once

// Classic getInventorySlot wire ↔ ProtocolInventoryItem (schema source: qtclient classic).

#include "protocol/protocolclassicwire.h"

namespace tibia {
namespace protobuf {
namespace protocol {
class ProtocolInventoryItem;
}
}
}

namespace tibia {
namespace protocol {

/// Mirror ProtocolGame::getInventorySlot feature gates into `out`.
/// When `presetItemId != 0`, skips reading u16 id (same as getInventorySlot id arg).
/// Returns false if item id cannot be resolved (caller should stop consuming).
bool readInventoryItem(ClassicReader& r, ::tibia::protobuf::protocol::ProtocolInventoryItem* out,
                       int presetItemId = 0);

/// Inverse of readInventoryItem for translateToClassic.
void writeInventoryItem(ClassicWriter& w, const ::tibia::protobuf::protocol::ProtocolInventoryItem& item);

}
}
