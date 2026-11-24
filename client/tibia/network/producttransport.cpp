#include "network/producttransport.h"

#include "protocol/protocolclassicwire.h"

#include <utility>

namespace tibia::network {

namespace {

/// The product's extended opcode message (0x32).
constexpr uint8_t ExtendedOpcodeMessage = 0x32;

}

TProductTransport::TProductTransport(FrameWriter writer)
    : m_writer(std::move(writer))
{
}

bool TProductTransport::send(const uint8_t opcode, const std::string& buffer) const
{
    if (!m_open || !m_writer)
        return false;
    protocol::ClassicWriter w;
    w.u8(ExtendedOpcodeMessage);
    w.u8(opcode);
    w.string(buffer);
    m_writer(w.out);
    return true;
}

}
