#pragma once

// An object on the classic wire: its appearance id, then the extra data of the
// one appearance kind it belongs to.

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

/// Reads one object into `out`. When `presetItemId != 0` the id was already read.
/// Returns false when the message ends inside the object.
bool readInventoryItem(ClassicReader& r, ::tibia::protobuf::protocol::ProtocolInventoryItem* out,
                       int presetItemId = 0);

/// Inverse of readInventoryItem for translateToClassic.
void writeInventoryItem(ClassicWriter& w, const ::tibia::protobuf::protocol::ProtocolInventoryItem& item);

}
}
