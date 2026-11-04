#pragma once

#include <framework/core/gamehooks.h>
#include <tibia/game/tgamenotify.h>

#include <string_view>

namespace tibia::game {

/// CipSoft notify scope for session / protocol → controller hooks.
inline constexpr std::string_view kNotifyScope = "cip";

/// Protocol / engine → qtclient controllers via `TGameNotify` (CipSoft field bus).
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

} // namespace tibia::game
