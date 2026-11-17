#pragma once

#include "sprites/spritesheetsmetadatamanager.h"

#include <cip_gen.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace tibia::sprites {
class TAppearanceTypeSpritesheetMapping;
}

namespace tibia {
namespace appearances {

class TAppearanceType;
enum class EAppearanceCategory : int;

/// Counts the live instances of each appearance type and of each spritesheet
/// they draw from (CIP appearances::TAppearanceInstanceTracker). The first
/// instance of a type asks for its spritesheets to be loaded. Thread safe.
class TAppearanceInstanceTracker final : public cip::tibia::appearances::TAppearanceInstanceTracker
{
    Q_OBJECT
public:
    explicit TAppearanceInstanceTracker(std::shared_ptr<sprites::TAppearanceTypeSpritesheetMapping> mapping);
    ~TAppearanceInstanceTracker() override;

    /// An instance of `type` was created; the type's first one emits
    /// loadSpritesheetWithID for each of its spritesheets.
    void instanceCreated(const TAppearanceType& type);
    /// Emits loadSpritesheetWithID once for each spritesheet of `types`, in
    /// spritesheet order.
    void loadSpritesheetsOf(const std::set<const TAppearanceType*>& types);
    /// An instance of `type` was destroyed; going below zero is reported and
    /// ignored.
    void instanceDestroyed(const TAppearanceType& type);
    /// instanceDestroyed of the type `category` / `id` (the type may be gone).
    void instanceDestroyed(EAppearanceCategory category, uint16_t id);
    /// The live instances of `type`.
    int instanceCount(const TAppearanceType& type);

Q_SIGNALS:
    void loadSpritesheetWithID(tibia::sprites::TSpritesheetID SpritesheetID);

private:
    std::map<EAppearanceCategory, std::map<uint16_t, int>> m_instanceCounts;
    std::recursive_mutex m_instanceCountsMutex;
    std::map<sprites::TSpritesheetID, int> m_spritesheetCounts;
    std::recursive_mutex m_spritesheetCountsMutex;
    std::shared_ptr<sprites::TAppearanceTypeSpritesheetMapping> m_mapping;
};

}
}
