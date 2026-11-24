#pragma once

#include <tibia/game/tgamenotify.h>

#include <any>
#include <string_view>

namespace tibia {
namespace game {

/// Publishes `field` on the `TGameNotify` string bus (the pack ABI's fields).
/// Tip code connects to the CipSoft classes' Qt signals instead.
template<typename... T>
inline void notify(const std::string_view field, const T&... args)
{
    TGameNotify::instance()->publish(field, TGameNotify::Args{ std::any(args)... });
}

template<typename R, typename... T>
inline R notifyR(const std::string_view field, const T&... args)
{
    R result{};
    const std::any out = TGameNotify::instance()->publish(field, TGameNotify::Args{ std::any(args)... });
    if (const R* value = std::any_cast<R>(&out))
        result = *value;
    return result;
}

}
}
