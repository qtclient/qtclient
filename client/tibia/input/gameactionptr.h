#pragma once

#include <memory>

namespace tibia::input {
/// The game action handle a publishGameAction signal carries (see gameaction.h).
class IGameAction;
using IGameActionPtr = std::shared_ptr<const IGameAction>;
}
