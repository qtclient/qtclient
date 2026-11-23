#include "premiummessage.h"

#include <QtGlobal>

#include <iterator>

namespace tibia::game {

namespace {

/// The premium panel's texts by feature (13 has a placeholder: its line is a
/// TPremiumMessageRenewPremium).
constexpr const char* MessageIds[] = {
    "premium_feature_areas",          "premium_feature_offlinetraining", "premium_feature_spells",
    "premium_feature_mounts",         "premium_feature_outfits",         "premium_feature_xp_boost",
    "premium_feature_death_penalty",  "premium_feature_market",          "premium_feature_house",
    "premium_feature_large_vip_list", "premium_feature_depot",           "premium_feature_private_chat",
    "premium_feature_promotion",      "dummy_unknown",                   "premium_feature_arena",
    "premium_feature_prey_slots",     "premium_feature_imbuing",         "premium_feature_daily_reward",
    "premium_feature_analysers",      "premium_all_quests",              "premium_no_login_queue",
    "premium_powerful_summons",       "premium_guild_management",        "premium_outfit_addons",
    "premium_premium_only_worlds",    "premium_regenerate_faster",       "premium_loyalty_system",
    "premium_track_more_items",       "premium_more_soul_points",        "premium_more_vip_groups",
    "premium_track_more_quests",      "premium_refined_quick_loot",      "premium_more_charms",
    "premium_secondary_battle_lists", "premium_use_team_finder",         "premium_use_skill_wheel",
};

}

const char* TPremiumMessage::messageId(const uint8_t feature)
{
    return feature < std::size(MessageIds) ? MessageIds[feature] : nullptr;
}

QString TPremiumMessage::smallIconUrl(const uint8_t feature)
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("premium-icons-20x20")).arg(feature);
}

QString TPremiumMessage::largeIconUrl(const uint8_t feature)
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("premium-icons-32x32")).arg(feature);
}

TPremiumMessage::TPremiumMessage(const uint8_t feature, const bool largeIcon, QObject* parent)
    : cip::tibia::game::TPremiumMessage(parent)
    , m_feature(feature)
{
    if (const char* id = messageId(feature))
        m_message = qtTrId(id);
    m_imageUrl = largeIcon ? largeIconUrl(feature) : smallIconUrl(feature);
}

}
