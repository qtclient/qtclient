#include "cyclopedia/generalplayerstatisticsprovider.h"

#include <protocol.pb.h>

#include <gtest/gtest.h>

using tibia::cyclopedia::TGeneralPlayerStatisticsProvider;
using tibia::game::SkillType;
using tibia::protobuf::protocol::ProtocolCharacterInfoGeneralStats;

namespace {

std::unique_ptr<ProtocolCharacterInfoGeneralStats> statistics()
{
    auto s = std::make_unique<ProtocolCharacterInfoGeneralStats>();
    s->set_level(120);
    s->set_level_percent(4567);
    s->set_base_xp_gain(150);
    s->set_low_level_xp_bonus(0);
    s->set_xp_boost_percent(50);
    s->set_stamina_xp_bonus(150);
    s->set_trailing_first(25);
    s->set_trailing_second(600);
    auto* magic = s->add_skills();
    magic->set_skill_id(1);
    magic->set_level(80);
    magic->set_base_level(75);
    magic->set_loyalty_level(78);
    magic->set_percent(1234);
    auto* club = s->add_skills();
    club->set_skill_id(9);
    club->set_level(90);
    auto* element = s->add_specialized_magic_levels();
    element->set_element(1);
    element->set_value(3);
    return s;
}

}

TEST(GeneralPlayerStatisticsProvider, SkillsByCipSkillId)
{
    const TGeneralPlayerStatisticsProvider provider(statistics());
    const auto magic = provider.skill(static_cast<SkillType>(4));
    EXPECT_DOUBLE_EQ(magic.level, 80);
    EXPECT_DOUBLE_EQ(magic.baseLevel, 75);
    EXPECT_DOUBLE_EQ(magic.loyaltyLevel, 78);
    EXPECT_NEAR(magic.percent, 12.34, 1e-5);
    // Club (12) is id 9, sword (13) id 8.
    EXPECT_DOUBLE_EQ(provider.skill(static_cast<SkillType>(12)).level, 90);
    EXPECT_DOUBLE_EQ(provider.skill(static_cast<SkillType>(13)).level, 0);
    // Types without values answer zeros.
    EXPECT_DOUBLE_EQ(provider.skill(static_cast<SkillType>(21)).level, 0);
    EXPECT_DOUBLE_EQ(provider.skill(static_cast<SkillType>(5)).level, 0);
}

TEST(GeneralPlayerStatisticsProvider, RatesAndPercent)
{
    const TGeneralPlayerStatisticsProvider provider(statistics());
    EXPECT_NEAR(provider.levelPercent(), 45.67f, 1e-4);
    const auto& rates = provider.experienceRates();
    EXPECT_DOUBLE_EQ(rates.base, 1.5);
    EXPECT_DOUBLE_EQ(rates.storeBoost, 0.5);
    EXPECT_DOUBLE_EQ(rates.dailyRewardBoost, 0.25);
    EXPECT_DOUBLE_EQ(rates.staminaMultiplier, 1.5);
    EXPECT_EQ(provider.dailyRewardXpBoostSeconds(), 600u);
    ASSERT_EQ(provider.specialMagicLevels().size(), 1u);
    EXPECT_EQ(provider.specialMagicLevels()[0], std::make_pair(1, 3));
}
