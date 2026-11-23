#pragma once

#include <exception>
#include <string>
#include <utility>

namespace tibia {
namespace common {

/// The error of the English pluraliser (`plural`): it carries the message
/// and reports it from `what`.
class TPluralException : public std::exception
{
public:
    explicit TPluralException(std::string message)
        : m_message(std::move(message))
    {}

    const char* what() const noexcept override { return m_message.c_str(); }

private:
    std::string m_message;
};

}
}
