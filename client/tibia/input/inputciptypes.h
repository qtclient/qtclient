#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <cstdint>
#include <string>

namespace tibia::input {

/// Own CIP-named types under `parent` (keyboard shortcut handler).
void ownProductSurface(QObject* parent);

class TKeyEventHandler final : public cip::tibia::input::TKeyEventHandler
{
    Q_OBJECT
public:
    using cip::tibia::input::TKeyEventHandler::TKeyEventHandler;
};

class TKeyboardInputToGameActionConverter final : public cip::tibia::input::TKeyboardInputToGameActionConverter
{
    Q_OBJECT
public:
    using cip::tibia::input::TKeyboardInputToGameActionConverter::TKeyboardInputToGameActionConverter;
};

class TKeyboardShortcutHandlerEventFilter final : public cip::tibia::input::TKeyboardShortcutHandlerEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::input::TKeyboardShortcutHandlerEventFilter::TKeyboardShortcutHandlerEventFilter;
};

class TKeyboardStatusStorage final : public cip::tibia::input::TKeyboardStatusStorage
{
    Q_OBJECT
public:
    using cip::tibia::input::TKeyboardStatusStorage::TKeyboardStatusStorage;
};

class TPlayerMovementIntentHandler final : public cip::tibia::input::TPlayerMovementIntentHandler
{
    Q_OBJECT
public:
    using cip::tibia::input::TPlayerMovementIntentHandler::TPlayerMovementIntentHandler;
};

class TRecordKeyboardShortcutHandler final : public cip::tibia::input::TRecordKeyboardShortcutHandler
{
    Q_OBJECT
public:
    using cip::tibia::input::TRecordKeyboardShortcutHandler::TRecordKeyboardShortcutHandler;
};

/// CIP location / appearance action contexts — carry where an action targets.
class TLocationUnknownContext final : public ::tibia::CipTyped
{
public:
    TLocationUnknownContext() = default;
};

class TLocationWorldMapCoordinateContext final : public ::tibia::CipTyped
{
public:
    explicit TLocationWorldMapCoordinateContext(int x = 0, int y = 0, int z = 0)
        : m_x(x), m_y(y), m_z(z)
    {
    }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }

private:
    int m_x = 0, m_y = 0, m_z = 0;
};

class TLocationWorldMapCoordinateWithStackPositionContext final : public ::tibia::CipTyped
{
public:
    TLocationWorldMapCoordinateWithStackPositionContext(int x = 0, int y = 0, int z = 0, int stack = -1)
        : m_x(x), m_y(y), m_z(z), m_stack(stack)
    {
    }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }
    int stackPosition() const { return m_stack; }

private:
    int m_x = 0, m_y = 0, m_z = 0, m_stack = -1;
};

class TLocationWorldMapGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TLocationWorldMapGameActionContext(int x = 0, int y = 0, int z = 0)
        : m_x(x), m_y(y), m_z(z)
    {
    }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }

private:
    int m_x = 0, m_y = 0, m_z = 0;
};

class TLocationActionButtonGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TLocationActionButtonGameActionContext(int barId = 0, int buttonId = 0)
        : m_barId(barId), m_buttonId(buttonId)
    {
    }
    int barId() const { return m_barId; }
    int buttonId() const { return m_buttonId; }

private:
    int m_barId = 0, m_buttonId = 0;
};

class TLocationBattleListGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TLocationBattleListGameActionContext(uint32_t creatureId = 0)
        : m_creatureId(creatureId)
    {
    }
    uint32_t creatureId() const { return m_creatureId; }

private:
    uint32_t m_creatureId = 0;
};

class TLocationContainerWithSlotGameActionContext final : public ::tibia::CipTyped
{
public:
    TLocationContainerWithSlotGameActionContext(int containerId = 0, int slot = 0)
        : m_containerId(containerId), m_slot(slot)
    {
    }
    int containerId() const { return m_containerId; }
    int slot() const { return m_slot; }

private:
    int m_containerId = 0, m_slot = 0;
};

class TLocationDepotSearchGameActionContext final : public ::tibia::CipTyped {};
class TLocationDepotSearchDepotItemGameActionContext final : public ::tibia::CipTyped {};
class TLocationDepotSearchInboxItemGameActionContext final : public ::tibia::CipTyped {};
class TLocationInventoryGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TLocationInventoryGameActionContext(int slot = 0)
        : m_slot(slot)
    {
    }
    int slot() const { return m_slot; }

private:
    int m_slot = 0;
};
class TLocationNPCChatKeywordGameActionContext final : public ::tibia::CipTyped {};
class TLocationNpcTradeGameActionContext final : public ::tibia::CipTyped {};
class TLocationPlayerTradeGameActionContext final : public ::tibia::CipTyped {};
class TLocationTutorialHintDialogGameActionContext final : public ::tibia::CipTyped {};

class TDecoItemKitObjectApperanceTypeGameActionContext final : public ::tibia::CipTyped {};
class TLiquidObjectApperanceTypeGameActionContext final : public ::tibia::CipTyped {};
class TObjectApperanceTypeGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TObjectApperanceTypeGameActionContext(uint16_t appearanceId = 0)
        : m_appearanceId(appearanceId)
    {
    }
    uint16_t appearanceId() const { return m_appearanceId; }

private:
    uint16_t m_appearanceId = 0;
};
class TObjectCreatureGameActionContext final : public ::tibia::CipTyped
{
public:
    explicit TObjectCreatureGameActionContext(uint32_t creatureId = 0)
        : m_creatureId(creatureId)
    {
    }
    uint32_t creatureId() const { return m_creatureId; }

private:
    uint32_t m_creatureId = 0;
};
class TUpgradeableObjectApperanceTypeGameActionContext final : public ::tibia::CipTyped {};

class TGenericChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    TGenericChatGameActionProvider() = default;
};

/// CIP movement intents consumed by TPlayerMovementInputInterpreter / pathing.
class TMovementIntentDirection final : public ::tibia::CipTyped
{
public:
    explicit TMovementIntentDirection(int direction = 0)
        : m_direction(direction)
    {
    }
    int direction() const { return m_direction; }

private:
    int m_direction = 0;
};

class TMovementIntentRotate final : public ::tibia::CipTyped
{
public:
    explicit TMovementIntentRotate(int direction = 0)
        : m_direction(direction)
    {
    }
    int direction() const { return m_direction; }

private:
    int m_direction = 0;
};

class TMovementIntentStop final : public ::tibia::CipTyped
{
public:
    TMovementIntentStop() = default;
};

class TMovementIntentTargetCoordinate final : public ::tibia::CipTyped
{
public:
    TMovementIntentTargetCoordinate(int x = 0, int y = 0, int z = 0)
        : m_x(x), m_y(y), m_z(z)
    {
    }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }

private:
    int m_x = 0, m_y = 0, m_z = 0;
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

class TPlayerMovementIntent final : public ::tibia::CipTyped
{
public:
    enum class Kind { Stop, Direction, Rotate, Target };
    TPlayerMovementIntent() = default;
    static TPlayerMovementIntent stop()
    {
        TPlayerMovementIntent intent;
        intent.m_kind = Kind::Stop;
        return intent;
    }
    static TPlayerMovementIntent direction(const int dir)
    {
        TPlayerMovementIntent intent;
        intent.m_kind = Kind::Direction;
        intent.m_direction = dir;
        return intent;
    }
    static TPlayerMovementIntent rotate(const int dir)
    {
        TPlayerMovementIntent intent;
        intent.m_kind = Kind::Rotate;
        intent.m_direction = dir;
        return intent;
    }
    static TPlayerMovementIntent target(const int x, const int y, const int z)
    {
        TPlayerMovementIntent intent;
        intent.m_kind = Kind::Target;
        intent.m_x = x;
        intent.m_y = y;
        intent.m_z = z;
        return intent;
    }
    Kind kind() const { return m_kind; }
    int direction() const { return m_direction; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }

private:
    Kind m_kind = Kind::Stop;
    int m_direction = 0;
    int m_x = 0, m_y = 0, m_z = 0;
};

} // namespace tibia::input
