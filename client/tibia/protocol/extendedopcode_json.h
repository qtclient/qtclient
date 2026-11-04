#pragma once

/// Pack-facing JSON extended-opcode send (CIP S/P/E chunking).
/// Prefer this over `#include "network/extendedopcoderouter.h"`.

#include "tibia/protocol/extendedopcode.h"

#include <QJsonValue>

#include <cstdint>

namespace tibia::protocol {

/// Crystal NetworkMessage::getString decodes ISO-8859-1; chunks at 65000 bytes.
bool sendExtendedJsonOpcode(uint8_t opcode, const QJsonValue& data);

} // namespace tibia::protocol
