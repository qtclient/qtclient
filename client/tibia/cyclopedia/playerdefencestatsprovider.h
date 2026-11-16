#pragma once

#include "game/defencestatsprovider.h"

#include <client/staticdata.h>

namespace tibia {
namespace cyclopedia {

/// The defence stats of the character info message (CIP:
/// cyclopedia::TPlayerDefenceStatsProvider, made by TCharacterInfoStorage).
class TPlayerDefenceStatsProvider final : public game::IDefenceStatsProvider
{
public:
    explicit TPlayerDefenceStatsProvider(CyclopediaCharacterDefenceStats stats);

    int armor() const override;
    int mantra() const override;
    int defence() const override;
    std::optional<int> defence(ECombatStatSource source) const override;
    std::optional<int> defenceSkill() const override;
    double mitigation() const override;
    std::optional<double> mitigation(ECombatStatSource source) const override;
    std::optional<int> mitigationSkill() const override;
    std::vector<ElementalResistance> elementalResistances() const override;
    std::vector<ElementalResistance> elementalResistanceSources(int damageType) const override;
    uint32_t magicShieldCapacity() const override;
    uint32_t magicShieldCapacityFlat() const override;
    double magicShieldCapacityPercent() const override;
    int damageReflection() const override;
    double dodge() const override;
    std::optional<double> dodge(ECombatStatSource source) const override;

private:
    CyclopediaCharacterDefenceStats m_stats;
};

}
}
