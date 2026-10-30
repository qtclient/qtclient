#include "tibia/protocol/extendedopcode.h"

#include <utility>

namespace tibia::protocol {

ExtendedOpcodeReader& ExtendedOpcodeReader::instance()
{
    static ExtendedOpcodeReader reader;
    return reader;
}

int ExtendedOpcodeReader::subscribe(Listener listener)
{
    const int id = m_next++;
    m_listeners.emplace_back(id, std::move(listener));
    return id;
}

void ExtendedOpcodeReader::unsubscribe(const int id)
{
    std::erase_if(m_listeners, [id](const auto& entry) { return entry.first == id; });
}

void ExtendedOpcodeReader::publish(const uint8_t opcode, const std::string& buffer)
{
    // Copy: a listener may subscribe or unsubscribe while publishing.
    const auto listeners = m_listeners;
    for (const auto& [id, listener] : listeners) {
        if (listener)
            listener(opcode, buffer);
    }
}

void ExtendedOpcodeReader::setSender(Sender sender) { m_sender = std::move(sender); }

bool ExtendedOpcodeReader::send(const uint8_t opcode, const std::string& buffer) const
{
    return m_sender && m_sender(opcode, buffer);
}

}
