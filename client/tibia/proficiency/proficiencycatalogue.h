#pragma once

#include "proficiency/proficiencystorage.h"

#include <optional>
#include <vector>

namespace tibia {
namespace proficiency {

/// CIP : the perk of a shaping catalogue index (the server's
/// "modifierEnum"), empty for an unused index.
std::optional<TProficiencyPerk> shapePerk(int index);
/// CIP : the perk's magnitude at refine rank `rank` (0..10).
TProficiencyPerk perkAtRank(TProficiencyPerk perk, int rank);
/// CIP (profession, 0): the catalogue indices a profession
/// (1 knight .. 5 monk) can be shaped into; empty for other professions.
std::vector<int> shapeOptions(int profession);
/// The shaped perk of a catalogue index at a rank (CIP).
TProficiencyPerk shapedPerk(int index, int rank);

}
}
