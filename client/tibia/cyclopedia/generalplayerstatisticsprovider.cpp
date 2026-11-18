#include "generalplayerstatisticsprovider.h"

#include <protocol.pb.h>

namespace tibia::cyclopedia {

namespace {

/// The skill id the answer lists `type` under; -1 for a type it does not list.
int skillId(const game::SkillType type)
{
    static constexpr int ids[] = { 1, -1, -1, -1, -1, -1, 6, 7, 9, 8, 10, 11, 13 };
    const auto index = static_cast<unsigned>(static_cast<int>(type) - 4);
    return index < std::size(ids) ? ids[index] : -1;
}

}

TGeneralPlayerStatisticsProvider::TGeneralPlayerStatisticsProvider(
    std::unique_ptr<protobuf::protocol::ProtocolCharacterInfoGeneralStats> statistics)
    : m_statistics(std::move(statistics))
{
    const auto& s = *m_statistics;
    // The answer sends the rates in percent; the daily reward boost is its second-last value.
    m_experienceRates.base = s.base_xp_gain() / 100.0;
    m_experienceRates.lowLevelBonus = s.low_level_xp_bonus() / 100.0;
    m_experienceRates.storeBoost = s.xp_boost_percent() / 100.0;
    m_experienceRates.dailyRewardBoost = s.trailing_first() / 100.0;
    m_experienceRates.staminaMultiplier = s.stamina_xp_bonus() / 100.0;
    for (const auto& entry : s.specialized_magic_levels())
        m_specialMagicLevels.emplace_back(static_cast<int>(entry.element()), entry.value());
}

TGeneralPlayerStatisticsProvider::~TGeneralPlayerStatisticsProvider() = default;

uint32_t TGeneralPlayerStatisticsProvider::hitPoints() const { return m_statistics->health(); }
uint32_t TGeneralPlayerStatisticsProvider::maxHitPoints() const { return m_statistics->max_health(); }
uint32_t TGeneralPlayerStatisticsProvider::mana() const { return m_statistics->mana(); }
uint32_t TGeneralPlayerStatisticsProvider::maxMana() const { return m_statistics->max_mana(); }
uint8_t TGeneralPlayerStatisticsProvider::soulPoints() const { return static_cast<uint8_t>(m_statistics->soul()); }

game::IGeneralStatsProvider::TSpeed TGeneralPlayerStatisticsProvider::speed() const
{
    return { static_cast<uint16_t>(m_statistics->speed()), static_cast<uint16_t>(m_statistics->base_speed()) };
}

uint64_t TGeneralPlayerStatisticsProvider::experience() const { return m_statistics->experience(); }
uint16_t TGeneralPlayerStatisticsProvider::level() const { return static_cast<uint16_t>(m_statistics->level()); }
/// The answer sends hundredths of a percent.
float TGeneralPlayerStatisticsProvider::levelPercent() const { return m_statistics->level_percent() / 100.0f; }

const game::IGeneralStatsProvider::TExperienceRates& TGeneralPlayerStatisticsProvider::experienceRates() const
{
    return m_experienceRates;
}

uint32_t TGeneralPlayerStatisticsProvider::freeCapacity() const { return m_statistics->free_capacity(); }
uint32_t TGeneralPlayerStatisticsProvider::totalCapacity() const { return m_statistics->capacity(); }
uint32_t TGeneralPlayerStatisticsProvider::baseCapacity() const { return m_statistics->base_capacity(); }

game::IGeneralStatsProvider::TSkillValues TGeneralPlayerStatisticsProvider::skill(const game::SkillType type) const
{
    const auto value = static_cast<unsigned>(type);
    if (value - 21u > 3u) {
        const int id = skillId(type);
        for (const auto& entry : m_statistics->skills()) {
            if (static_cast<int>(entry.skill_id()) != id)
                continue;
            return { static_cast<double>(entry.level()), static_cast<double>(entry.base_level()),
                     static_cast<double>(entry.loyalty_level()), static_cast<double>(entry.percent() / 100.0f) };
        }
    }
    return {};
}

const std::vector<std::pair<int, int>>& TGeneralPlayerStatisticsProvider::specialMagicLevels() const
{
    return m_specialMagicLevels;
}

uint32_t TGeneralPlayerStatisticsProvider::staminaMinutes() const { return m_statistics->stamina_minutes(); }
uint32_t TGeneralPlayerStatisticsProvider::regenerationSeconds() const { return m_statistics->regeneration(); }
uint32_t TGeneralPlayerStatisticsProvider::offlineTrainingMinutes() const { return m_statistics->offline_training(); }
uint32_t TGeneralPlayerStatisticsProvider::storeXpBoostSeconds() const { return m_statistics->xp_boost_remaining(); }
bool TGeneralPlayerStatisticsProvider::canBuyXpBoost() const { return m_statistics->can_buy_xp_boost(); }
uint32_t TGeneralPlayerStatisticsProvider::dailyRewardXpBoostSeconds() const { return m_statistics->trailing_second(); }

}
