#include "traderstorage.h"

#include "appearances/appearancetype.h"

namespace tibia::game {

void TTraderStorage::addAppearanceTypes(const std::vector<const appearances::TAppearanceType*>& appearanceTypes)
{
    for (const appearances::TAppearanceType* appearanceType : appearanceTypes) {
        for (const appearances::TNpcTradeData& npc : appearanceType->npcTradeData()) {
            if (npc.buyPrice != 0)
                m_objectTypesByTrader[npc.name].insert(appearanceType->getId());
        }
    }
}

const std::set<uint16_t>& TTraderStorage::objectTypesBoughtBy(const QString& npcName) const
{
    static const std::set<uint16_t> none;
    const auto it = m_objectTypesByTrader.find(npcName);
    return it != m_objectTypesByTrader.end() ? it->second : none;
}

}
