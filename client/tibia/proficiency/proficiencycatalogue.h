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
/// The catalogue indices a profession (1 knight .. 5 monk) can be shaped
/// into: the 34 common indices, then the profession's 5 x 6 augments; empty
/// for other professions. The alternate table of profession 1 leaves out the
/// fifth augment column (59 entries); the other professions' alternate tables
/// are the same as their first. The shape options dialog reads the first.
std::vector<int> shapeOptions(int profession, bool alternateTable);
/// The shaped perk of a catalogue index at a rank (CIP).
TProficiencyPerk shapedPerk(int index, int rank);

}
}
