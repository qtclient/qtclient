#pragma once

#include <framework/global.h>
#include <tibia/game/gamenotify.h>

#include <memory>
#include <string_view>

class GameObject;
using GameObjectPtr = std::shared_ptr<GameObject>;

/// Tip shared-object base. Former OTC Lua-field publishes (`onConnect` /
/// `onOpcode`) go through SoftHooks / `tibia::game::notify` — not GameEvents.
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    virtual ~GameObject() = default;

    template<typename R, typename... T>
    R callLuaField(const std::string_view field, const T&... args)
    {
        return tibia::game::notifyR<R>(field, args...);
    }
    template<typename... T>
    void callLuaField(const std::string_view field, const T&... args)
    {
        tibia::game::notify(field, args...);
    }

    GameObjectPtr asGameObject() { return shared_from_this(); }
    int getUseCount();

    template<typename T>
    std::shared_ptr<T> static_self_cast() { return std::static_pointer_cast<T>(shared_from_this()); }
};
