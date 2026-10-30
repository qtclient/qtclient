#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace tibia::protocol {

/// OT-only extended opcode (often 0x32). Parsers publish here. Subscribers are
/// the extended-opcode router and product JSON opcodes — not Lua onExtendedOpcode.
class ExtendedOpcodeReader
{
public:
    using Listener = std::function<void(uint8_t opcode, const std::string& buffer)>;

    static ExtendedOpcodeReader& instance();

    int subscribe(Listener listener);
    void unsubscribe(int id);
    void publish(uint8_t opcode, const std::string& buffer);

    /// Outbound extended opcodes. The live protocol installs this; product
    /// features send through the reader, not through the game session.
    using Sender = std::function<bool(uint8_t opcode, const std::string& buffer)>;
    void setSender(Sender sender);
    bool send(uint8_t opcode, const std::string& buffer) const;

private:
    std::vector<std::pair<int, Listener>> m_listeners;
    Sender m_sender;
    int m_next = 1;
};

}
