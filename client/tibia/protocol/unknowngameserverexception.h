#pragma once

#include <cstdint>
#include <exception>
#include <string>

namespace tibia {
namespace protocol {

/// CIP protocol::TUnknownGameserverException: a strict server message
/// processor read a message of an opcode the translator does not know.
class TUnknownGameserverException : public std::exception
{
public:
    explicit TUnknownGameserverException(uint8_t Opcode);
    ~TUnknownGameserverException() override;

    const char* what() const noexcept override;

    uint8_t opcode() const { return m_opcode; }

private:
    uint8_t m_opcode = 0;
    /// "Unknown Gameserver Message: <opcode>".
    std::string m_message;
};

}
}
