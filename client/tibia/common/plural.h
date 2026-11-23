#pragma once

#include <string>

namespace tibia {
namespace common {

/// The English plural of an object or creature name.
///
/// A leading "a " / "an " is dropped. A negative `count` returns the name
/// without its article and nothing else; `count` 1 (or a one-letter name)
/// returns the name unchanged. Otherwise the head noun is pluralised: the
/// words before a lone "of" (or everything before " with ", whose part after
/// it is pluralised too when it starts with an article), by a fixed list of
/// irregular and unchanging creature names, then by the English suffix rules.
/// `count` 0 gives the plural alone, any other count prefixes the number.
/// Throws TPluralException for an empty name.
std::string plural(const std::string& name, int count);

}
}
