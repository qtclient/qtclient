#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include "gameactioncontext.h"

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

/// CIP movement intents consumed by TPlayerMovementInputInterpreter / pathing.
/// Kind tags match CIP `TMovementIntent*::v2` return values (pin unit).
enum class TMovementIntentKind : int {
    Target = 0,
    Direction = 1,
    Rotate = 2,
    Stop = 3,
};

/// CIP's movement intent interface: what the player's movement input asks for.
class TMovementIntent
{
public:
    virtual ~TMovementIntent() = default;
    /// CIP v2: the intent's kind.
    virtual int v2() const = 0;
    TMovementIntentKind kind() const { return static_cast<TMovementIntentKind>(v2()); }
};
using TMovementIntentPtr = std::shared_ptr<const TMovementIntent>;

/// A step direction (+0x8): tibia::client::Direction.
class TMovementIntentDirection final : public TMovementIntent
{
public:
    explicit TMovementIntentDirection(int direction = 0)
        : m_direction(direction)
    {
    }
    /// CIP `TMovementIntentDirection::v2` → 1.
    static constexpr TMovementIntentKind kindTag() { return TMovementIntentKind::Direction; }
    int v2() const override { return static_cast<int>(kindTag()); }
    int direction() const { return m_direction; }

private:
    int m_direction = 0;
};

class TMovementIntentRotate final : public TMovementIntent
{
public:
    explicit TMovementIntentRotate(int direction = 0)
        : m_direction(direction)
    {
    }
    /// CIP `TMovementIntentRotate::v2` → 2.
    static constexpr TMovementIntentKind kindTag() { return TMovementIntentKind::Rotate; }
    int v2() const override { return static_cast<int>(kindTag()); }
    int direction() const { return m_direction; }

private:
    int m_direction = 0;
};

class TMovementIntentStop final : public TMovementIntent
{
public:
    TMovementIntentStop() = default;
    /// CIP `TMovementIntentStop::v2` → 3.
    static constexpr TMovementIntentKind kindTag() { return TMovementIntentKind::Stop; }
    int v2() const override { return static_cast<int>(kindTag()); }
};

class TMovementIntentTargetCoordinate final : public TMovementIntent
{
public:
    TMovementIntentTargetCoordinate(int x = 0, int y = 0, int z = 0, bool enterUnwalkableGoal = false)
        : m_x(x), m_y(y), m_z(z), m_enterUnwalkableGoal(enterUnwalkableGoal)
    {
    }
    /// CIP `TMovementIntentTargetCoordinate::v2` → 0.
    static constexpr TMovementIntentKind kindTag() { return TMovementIntentKind::Target; }
    int v2() const override { return static_cast<int>(kindTag()); }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }
    /// Path planning may end on the goal even when it cannot be walked on.
    bool enterUnwalkableGoal() const { return m_enterUnwalkableGoal; }

private:
    int m_x = 0, m_y = 0, m_z = 0;
    bool m_enterUnwalkableGoal = false;
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
