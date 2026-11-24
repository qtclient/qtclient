#pragma once

#include <exception>
#include <string>

/// The exception CipSoft's bounds-checked containers of the movement planner
/// throw. The message reads "<file name> <line>: <message>", followed by
/// " (<name> = <value>)" for each named value that is given and by
/// ", reason:\n<reason>" when a reason is given.
class TMyException : public std::exception
{
public:
    TMyException(const std::string& file, int line, const std::string& message, const std::string& firstName = {},
                 int firstValue = 0, const std::string& secondName = {}, int secondValue = 0,
                 const std::string& reason = {});

    const char* what() const noexcept override { return m_message.c_str(); }
    /// "<file name> <line>".
    const std::string& location() const { return m_location; }

private:
    std::string m_message;
    std::string m_location;
};
