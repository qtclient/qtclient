#pragma once

#include <QByteArray>

#include <cstdint>
#include <functional>
#include <string>

namespace tibia::network {

/// Host (pack surface, no CipSoft counterpart): the product's extended-opcode
/// transport. A login asks for it in the RSA padding when a pack listens for
/// extended opcodes (TLoginProtocolMessageHandler::setProductTransport); the
/// server opens it with extended opcode 0. Its frames (0x32, opcode, string)
/// are written to the live game server session's connection beside CipSoft's
/// protocol queue, which has no message for them. Packs reach it through
/// protocol::ExtendedOpcodeReader::send; the live session installs it there.
class TProductTransport
{
public:
    /// Writes one product frame to the session's connection.
    using FrameWriter = std::function<void(const QByteArray& frame)>;

    explicit TProductTransport(FrameWriter writer);

    /// The server opened the transport (extended opcode 0) or the session ended it.
    void setOpen(bool open) { m_open = open; }
    bool isOpen() const { return m_open; }
    /// False while the transport is not open.
    bool send(uint8_t opcode, const std::string& buffer) const;

private:
    FrameWriter m_writer;
    bool m_open = false;
};

}
