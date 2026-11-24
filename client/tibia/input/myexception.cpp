#include "input/myexception.h"

#include <ostream>
#include <sstream>

namespace {

/// The part of a path after its last '/' or '\'; the whole path when it has no
/// separator or ends in one.
std::string fileNameOfPath(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    if (separator == std::string::npos || separator + 1 >= path.size())
        return path;
    return path.substr(separator + 1);
}

}

TMyException::TMyException(const std::string& file, const int line, const std::string& message,
                           const std::string& firstName, const int firstValue, const std::string& secondName,
                           const int secondValue, const std::string& reason)
{
    std::stringstream stream;
    stream << fileNameOfPath(file) << " " << line;
    m_location = stream.str();
    // The message keeps the location in front: the stream goes on after it.
    stream << ": " << message;
    if (!firstName.empty())
        stream << " (" << firstName << " = " << firstValue << ")";
    if (!secondName.empty())
        stream << " (" << secondName << " = " << secondValue << ")";
    if (!reason.empty())
        stream << ", reason:" << std::endl << reason;
    m_message = stream.str();
}
