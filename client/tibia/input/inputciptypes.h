#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include "gameactioncontext.h"
#include "playermovementintent.h"

#include <QObject>

#include <cstdint>
#include <memory>
#include <string>

namespace tibia {
namespace input {

/// Own CIP-named types under `parent` (keyboard shortcut handler).
void ownProductSurface(QObject* parent);

class TKeyboardInputToGameActionConverter final : public cip::tibia::input::TKeyboardInputToGameActionConverter
{
    Q_OBJECT
public:
    using cip::tibia::input::TKeyboardInputToGameActionConverter::TKeyboardInputToGameActionConverter;
};

class TGenericChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    TGenericChatGameActionProvider() = default;
};

class TPathPlanningFailedException final : public ::tibia::CipTyped
{
public:
    explicit TPathPlanningFailedException(std::string message = {})
        : m_message(std::move(message))
    {
    }
    const std::string& message() const { return m_message; }

private:
    std::string m_message;
};

}
}
