#include "zlibbasewrapper.h"

#include <sstream>

namespace tibia::shared {

std::string TZlibBaseWrapper::getLastErrorMessage() const
{
    return m_lastErrorMessage;
}

void TZlibBaseWrapper::setLastError(const int code, const std::string& message)
{
    std::ostringstream stream;
    stream << message << " (" << code << ")";
    m_lastErrorMessage = stream.str();
}

const std::vector<unsigned char>& TZlibBaseWrapper::syncFlushMarker()
{
    static const std::vector<unsigned char> marker{ 0x00, 0x00, 0xff, 0xff };
    return marker;
}

}
