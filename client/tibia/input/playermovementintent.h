#pragma once

#include <memory>
#include <ostream>

namespace tibia {
namespace input {

/// CIP movement intents consumed by TPlayerMovementInputInterpreter / pathing.
/// Kind tags match CIP `TMovementIntent*::v2` return values (pin unit).
enum class TMovementIntentKind : int {
    Target = 0,
    Direction = 1,
    Rotate = 2,
    Stop = 3,
};

/// CIP input::TPlayerMovementIntent: what the player's movement input asks for.
/// Writes the kind's CipSoft name (TARGET_COORDINATE, DIRECTION, ROTATION,
/// NONE); an unknown kind writes nothing.
inline std::ostream& operator<<(std::ostream& stream, const TMovementIntentKind kind)
{
    switch (kind) {
    case TMovementIntentKind::Target:
        return stream << "TARGET_COORDINATE";
    case TMovementIntentKind::Direction:
        return stream << "DIRECTION";
    case TMovementIntentKind::Rotate:
        return stream << "ROTATION";
    case TMovementIntentKind::Stop:
        return stream << "NONE";
    }
    return stream;
}

class TMovementIntent
{
public:
    virtual ~TMovementIntent() = default;
    /// CIP v2: the intent's kind.
    virtual int v2() const = 0;
    TMovementIntentKind kind() const { return static_cast<TMovementIntentKind>(v2()); }
    /// CIP v3: the same kind with the same direction, rotation or target
    /// (and the same unwalkable-goal choice); any two stops are equal.
    bool operator==(const TMovementIntent& other) const;
    /// CIP v4.
    bool operator!=(const TMovementIntent& other) const { return !(*this == other); }
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

inline bool TMovementIntent::operator==(const TMovementIntent& other) const
{
    if (kind() != other.kind())
        return false;
    switch (other.kind()) {
    case TMovementIntentKind::Rotate:
        return static_cast<const TMovementIntentRotate&>(*this).direction()
            == static_cast<const TMovementIntentRotate&>(other).direction();
    case TMovementIntentKind::Direction:
        return static_cast<const TMovementIntentDirection&>(*this).direction()
            == static_cast<const TMovementIntentDirection&>(other).direction();
    case TMovementIntentKind::Target: {
        const auto& self = static_cast<const TMovementIntentTargetCoordinate&>(*this);
        const auto& target = static_cast<const TMovementIntentTargetCoordinate&>(other);
        return self.x() == target.x() && self.y() == target.y() && self.z() == target.z()
            && self.enterUnwalkableGoal() == target.enterUnwalkableGoal();
    }
    case TMovementIntentKind::Stop:
        break;
    }
    return true;
}

}
}
