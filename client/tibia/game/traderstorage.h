#pragma once

#include <QString>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace tibia::appearances {
class TAppearanceType;
}

namespace tibia {
namespace game {

/// CIP game::TTraderStorage: for every NPC, the object types it buys (an NPC
/// trade entry of the object with a buy price).
class TTraderStorage
{
public:
    TTraderStorage() = default;
    virtual ~TTraderStorage() = default;

    /// Records the NPCs that buy each of `appearanceTypes`.
    virtual void addAppearanceTypes(const std::vector<const appearances::TAppearanceType*>& appearanceTypes);
    /// The object types `npcName` buys; an unknown NPC buys none.
    virtual const std::set<uint16_t>& objectTypesBoughtBy(const QString& npcName) const;

private:
    std::map<QString, std::set<uint16_t>> m_objectTypesByTrader;
};

}
}
