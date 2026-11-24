#include "protocol/unknowngameserverexception.h"

#include <QString>

namespace tibia::protocol {

TUnknownGameserverException::TUnknownGameserverException(const uint8_t Opcode)
    : m_opcode(Opcode)
{
    m_message = QString::fromUtf8("Unknown Gameserver Message: %1").arg(static_cast<uint>(m_opcode)).toStdString();
}

TUnknownGameserverException::~TUnknownGameserverException() = default;

const char* TUnknownGameserverException::what() const noexcept
{
    return m_message.c_str();
}

}
