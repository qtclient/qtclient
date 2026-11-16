#pragma once

#include "game/miscstatsprovider.h"

#include <client/staticdata.h>

namespace tibia {
namespace cyclopedia {

/// The misc stats of the character info message (CIP:
/// cyclopedia::TPlayerMiscStatsProvider, made by TCharacterInfoStorage).
/// The message's second and third double lists are transcendence and
/// amplification.
class TPlayerMiscStatsProvider final : public game::IMiscStatsProvider
{
public:
    explicit TPlayerMiscStatsProvider(CyclopediaCharacterMiscStats stats);

    int currentBlessings() const override;
    int maxBlessings() const override;
    std::vector<TimedEffect> activeConcoctions() const override;
    std::vector<TimedEffect> consumableCooldowns() const override;
    double momentum() const override;
    std::optional<double> momentum(ECombatStatSource source) const override;
    double transcendence() const override;
    std::optional<double> transcendence(ECombatStatSource source) const override;
    double amplification() const override;
    std::optional<double> amplification(ECombatStatSource source) const override;
    std::vector<SpellAugment> proficiencySpellAugments() const override;
    std::vector<SpellAugment> skillWheelSpellAugments() const override;
    std::vector<SpellAugment> equipmentSpellAugments() const override;

private:
    CyclopediaCharacterMiscStats m_stats;
};

}
}
