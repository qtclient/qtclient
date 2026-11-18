#pragma once

#include "game/generalstatsprovider.h"

#include <memory>

namespace tibia::protobuf::protocol {
class ProtocolCharacterInfoGeneralStats;
}

namespace tibia {
namespace cyclopedia {

/// The general stats of the character info answer (made by TCharacterInfoStorage). Keeps
/// the message; the experience rates and the specialised magic levels are converted once.
class TGeneralPlayerStatisticsProvider final : public game::IGeneralStatsProvider
{
public:
    explicit TGeneralPlayerStatisticsProvider(std::unique_ptr<protobuf::protocol::ProtocolCharacterInfoGeneralStats> statistics);
    ~TGeneralPlayerStatisticsProvider() override;

    uint32_t hitPoints() const override;
    uint32_t maxHitPoints() const override;
    uint32_t mana() const override;
    uint32_t maxMana() const override;
    uint8_t soulPoints() const override;
    TSpeed speed() const override;
    uint64_t experience() const override;
    uint16_t level() const override;
    float levelPercent() const override;
    const TExperienceRates& experienceRates() const override;
    uint32_t freeCapacity() const override;
    uint32_t totalCapacity() const override;
    uint32_t baseCapacity() const override;
    TSkillValues skill(game::SkillType type) const override;
    const std::vector<std::pair<int, int>>& specialMagicLevels() const override;
    uint32_t staminaMinutes() const override;
    uint32_t regenerationSeconds() const override;
    uint32_t offlineTrainingMinutes() const override;
    uint32_t storeXpBoostSeconds() const override;
    bool canBuyXpBoost() const override;
    uint32_t dailyRewardXpBoostSeconds() const override;

private:
    std::unique_ptr<protobuf::protocol::ProtocolCharacterInfoGeneralStats> m_statistics;
    std::vector<std::pair<int, int>> m_specialMagicLevels;
    TExperienceRates m_experienceRates;
};

}
}
