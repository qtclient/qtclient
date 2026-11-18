#pragma once

#include "sprites/spritesheetsmetadatamanager.h"

#include <map>
#include <set>

namespace tibia::appearances {
class TAppearanceType;
enum class EAppearanceCategory : int;
}

namespace tibia {
namespace sprites {

/// The spritesheets each appearance type draws from (CIP
/// sprites::TAppearanceTypeSpritesheetMapping), filled when the appearances
/// are post-processed.
class TAppearanceTypeSpritesheetMapping
{
public:
    /// Starts with an empty entry for each category.
    TAppearanceTypeSpritesheetMapping();
    virtual ~TAppearanceTypeSpritesheetMapping() = default;

    /// Stores the spritesheets of every sprite of `type`'s frame groups; sprites
    /// no spritesheet holds are left out.
    virtual void addAppearanceType(TSpritesheetsMetadataManager& metadata, const appearances::TAppearanceType& type);
    /// The spritesheets of `type`; none for a type that was not added.
    virtual const std::set<TSpritesheetID>& spritesheetIdsOf(const appearances::TAppearanceType& type) const;
    const std::set<TSpritesheetID>& spritesheetIdsOf(appearances::EAppearanceCategory category, uint16_t id) const;

private:
    std::map<appearances::EAppearanceCategory, std::map<uint16_t, std::set<TSpritesheetID>>> m_spritesheetIds;
};

}
}
