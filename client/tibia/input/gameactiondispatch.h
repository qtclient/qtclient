#pragma once

#include "gameaction.h"

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace tibia::input {

namespace detail {
template <typename F>
struct GameActionHandlerArgument : GameActionHandlerArgument<decltype(&F::operator())>
{
};
template <typename R, typename A>
struct GameActionHandlerArgument<R (*)(A)>
{
    using type = std::remove_cvref_t<A>;
};
template <typename C, typename R, typename A>
struct GameActionHandlerArgument<R (C::*)(A) const>
{
    using type = std::remove_cvref_t<A>;
};
template <typename C, typename R, typename A>
struct GameActionHandlerArgument<R (C::*)(A)>
{
    using type = std::remove_cvref_t<A>;
};
}

/// The cases of a controller's handleGameAction slot (CIP: the switch over
/// the action type in each handler's handleGameAction), one per action type.
/// A case takes IGameAction or the action's class; actions of another class
/// are not its. The slot is connected to the router's publishGameActionInternal.
class TGameActionDispatch
{
public:
    template <typename F>
    void on(const EGameAction type, F&& handler)
    {
        using Handler = std::decay_t<F>;
        if constexpr (std::is_invocable_v<Handler&, const IGameAction&>) {
            m_cases[type] = [handler = Handler(std::forward<F>(handler))](const IGameAction& action) mutable { handler(action); };
        } else {
            using Action = typename detail::GameActionHandlerArgument<Handler>::type;
            m_cases[type] = [handler = Handler(std::forward<F>(handler))](const IGameAction& action) mutable {
                if (const auto* typed = dynamic_cast<const Action*>(&action))
                    handler(*typed);
            };
        }
    }

    /// Runs the action's case; false when it has none.
    bool handle(const IGameAction& action) const
    {
        const auto it = m_cases.find(action.gameActionType());
        if (it == m_cases.end())
            return false;
        it->second(action);
        return true;
    }
    bool handle(const IGameActionPtr& action) const { return action && handle(*action); }

    bool handles(const EGameAction type) const { return m_cases.contains(type); }

private:
    std::unordered_map<EGameAction, std::function<void(const IGameAction&)>> m_cases;
};

}
