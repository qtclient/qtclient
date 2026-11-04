#pragma once

#include <format>
#include <type_traits>

// CIP: enums format as underlying integers (replaces fmt::format_as).
template <typename E>
    requires std::is_enum_v<E>
struct std::formatter<E> : std::formatter<std::underlying_type_t<E>> {
    auto format(E e, format_context& ctx) const
    {
        return std::formatter<std::underlying_type_t<E>>::format(
            static_cast<std::underlying_type_t<E>>(e), ctx);
    }
};
