#pragma once

#include <string>

namespace tibia::assets {

/// Catalog + appearances.dat into appearance records, and LZMA sprite sheets.
bool load(const std::string& directory);
/// staticdata-*.dat (CIP field numbers; cip/proto has no staticdata schema).
bool loadStaticData(const std::string& directory);
bool resolveProficiencies(const std::string& directory);

}
