#pragma once

#include "cyclopedia/combatstatsqmldata.h"

#include <cip_gen.h>

#include <map>
#include <utility>
#include <vector>

namespace tibia {
namespace game {

/// CIP game::TPlayerSkillStats — combat stats from PlayerSkills (signal
/// playerSkillStatsChanged). Authority for offence·defence·misc blocks.
class TPlayerSkillStats final : public cip::tibia::game::TPlayerSkillStats
{
    Q_OBJECT
public:
    explicit TPlayerSkillStats(QObject* parent = nullptr);

    const cyclopedia::CombatStats& combatStats() const { return m_stats; }

    void setFlatDamageHealing(uint16_t flatBonus);
    void setAttackInfo(uint16_t attackValue, uint8_t attackElement);
    void setConvertedDamage(double convertedDamage, uint8_t convertedElement);
    void setImbuements(double lifeLeech, double manaLeech, double critChance, double critDamage, double onslaught);
    void setDefenseInfo(uint16_t defense, uint16_t armor, uint16_t mantra, double mitigation, double dodge,
                        uint16_t damageReflection);
    void setCombatAbsorbValues(const std::map<uint8_t, double>& absorbValues);
    void setForgeBonuses(double momentum, double transcendence, double amplification);
    /// The magic level boosts per damage type (PlayerSkills modifiers).
    void setSpecialMagicLevels(std::vector<std::pair<int, int>> boosts) { m_specialMagicLevels = std::move(boosts); }
    const std::vector<std::pair<int, int>>& specialMagicLevels() const { return m_specialMagicLevels; }
    void clear();

private:
    void commit(const cyclopedia::CombatStats& next);

    cyclopedia::CombatStats m_stats;
    std::vector<std::pair<int, int>> m_specialMagicLevels;
};

void localPlayerSetSpecialMagicLevels(std::vector<std::pair<int, int>> boosts);
/// The magic level boosts per damage type (damage type, levels).
std::vector<std::pair<int, int>> localPlayerSpecialMagicLevels();

}
}
