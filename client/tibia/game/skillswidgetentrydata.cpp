#include "skillswidgetentrydata.h"

#include "client/gamecore.h"

#include <QLocale>

#include <algorithm>
#include <optional>
#include <tibia/game/gamesession.h>

namespace tibia::game {

namespace {

const QLocale& english()
{
    static const QLocale locale(QLocale::English);
    return locale;
}

QString number(const qint64 value) { return english().toString(value); }

double levelPercentForDisplay()
{
    return tibia::game::localPlayerLevelPercentHundredths() / 100.0;
}

bool hasLocalPlayerSession()
{
    return tibia::game::localPlayerId() != 0;
}

/// CIP : "<value> = <base> +<equipment> (+<n> Loyalty)" when the
/// skill differs from its base.
QString skillDetail(const qint64 value, const qint64 base, const qint64 loyaltyBase)
{
    if (value == base)
        return {};
    QString detail = number(value) + QStringLiteral(" = ") + number(base);
    if (value != loyaltyBase)
        detail += (value > loyaltyBase ? QStringLiteral(" +") : QStringLiteral(" ")) + number(value - loyaltyBase);
    if (loyaltyBase != base)
        detail += qtTrId("loyalty_bonus").arg(loyaltyBase > base ? QStringLiteral("+") : QString(), number(loyaltyBase - base), QString());
    return detail;
}

std::optional<tibia::client::Skill> otcSkill(const SkillType type)
{
    switch (type) {
        case SkillType::Fist: return tibia::client::Fist;
        case SkillType::Club: return tibia::client::Club;
        case SkillType::Sword: return tibia::client::Sword;
        case SkillType::Axe: return tibia::client::Axe;
        case SkillType::Distance: return tibia::client::Distance;
        case SkillType::Shielding: return tibia::client::Shielding;
        case SkillType::Fishing: return tibia::client::Fishing;
        default: return std::nullopt;
    }
}

}

TSkillsWidgetEntryData::TSkillsWidgetEntryData(QObject* parent)
    : cip::tibia::game::TSkillsWidgetEntryData(parent)
{
    m_entryVisible = true;
}

void TSkillsWidgetEntryData::assign(const SkillsWidgetEntry& entry)
{
    if (entry == m_entry)
        return;
    m_entry = entry;
    m_caption = entry.caption;
    m_valueString = entry.valueString;
    m_valueStringColor = entry.valueStringColor;
    m_tooltip = entry.tooltip;
    m_progressBarVisible = entry.progressBarVisible;
    m_progressBarColor = entry.progressBarColor;
    m_progressBarValue = entry.progressBarValue;
    m_separatorType = entry.separatorType;
    m_changeHighlight = entry.changeHighlight;
    m_showButton = entry.showButton;
    m_iconSource = entry.iconSource;
    m_iconTooltip = entry.iconTooltip;
    m_additionalValueString = entry.additionalValueString;
    m_entryVisible = entry.entryVisible;
    Q_EMIT changed();
}

QString skillName(const SkillType type)
{
    switch (type) {
        case SkillType::Experience: return qtTrId("experience") + QChar(0x9c) + qtTrId("experience_short");
        case SkillType::MagicLevel: return qtTrId("magic_level");
        case SkillType::Shielding: return qtTrId("shielding");
        case SkillType::Distance: return qtTrId("distance_fighting");
        case SkillType::Club: return qtTrId("club_fighting");
        case SkillType::Sword: return qtTrId("sword_fighting");
        case SkillType::Axe: return qtTrId("axe_fighting");
        case SkillType::Fist: return qtTrId("fist_fighting");
        case SkillType::Fishing: return qtTrId("fishing");
        case SkillType::Level: return qtTrId("level");
        case SkillType::ExperienceGain: return qtTrId("experience_gain");
        case SkillType::HitPoints: return qtTrId("hit_points");
        case SkillType::Mana: return qtTrId("mana");
        case SkillType::Speed: return qtTrId("speed");
        case SkillType::Capacity: return qtTrId("capacity");
        case SkillType::Food: return qtTrId("food");
        case SkillType::SoulPoints: return qtTrId("soul_points");
        case SkillType::Stamina: return qtTrId("stamina");
        case SkillType::OfflineTraining: return qtTrId("offline_training");
    }
    return {};
}

bool skillHasProgressBar(const SkillType type)
{
    const auto value = static_cast<uint>(type);
    return value < 0x15 && ((0x19fc14u >> value) & 1) != 0;
}

QString progressBarKey(const SkillType type)
{
    switch (type) {
        case SkillType::Level: return QStringLiteral("experience");
        case SkillType::Stamina: return QStringLiteral("stamina");
        case SkillType::OfflineTraining: return QStringLiteral("offlinetraining");
        case SkillType::MagicLevel: return QStringLiteral("magiclevel");
        case SkillType::Fist: return QStringLiteral("fistfighting");
        case SkillType::Club: return QStringLiteral("clubfighting");
        case SkillType::Sword: return QStringLiteral("swordfighting");
        case SkillType::Axe: return QStringLiteral("axefighting");
        case SkillType::Distance: return QStringLiteral("distancefighting");
        case SkillType::Shielding: return QStringLiteral("shielding");
        case SkillType::Fishing: return QStringLiteral("fishing");
        default: return {};
    }
}

QString skillIconSource(const SkillType type)
{
    int index = 0;
    switch (type) {
        case SkillType::Experience: index = 1; break;
        case SkillType::MagicLevel: index = 3; break;
        case SkillType::Shielding: index = 7; break;
        case SkillType::Distance: index = 2; break;
        case SkillType::Club: index = 5; break;
        case SkillType::Sword: index = 4; break;
        case SkillType::Axe: index = 6; break;
        case SkillType::Fist: index = 0; break;
        case SkillType::Fishing: index = 8; break;
        default: break;
    }
    return QStringLiteral("image://skills-icons/%1").arg(index);
}

qint64 experienceForLevel(const int level)
{
    const qint64 l = level;
    return (((l - 6) * l + 17) * l - 12) / 6 * 100;
}

SkillsWidgetEntry buildSkillEntry(const SkillType type)
{
    SkillsWidgetEntry entry;
    entry.caption = skillName(type);
    entry.iconSource = skillIconSource(type);
    entry.iconTooltip = entry.caption;
    entry.progressBarVisible = true;

    if (!hasLocalPlayerSession())
        return entry;

    if (type == SkillType::Experience) {
        const double percent = levelPercentForDisplay();
        const qint64 experience = static_cast<qint64>(tibia::game::localPlayerExperience());
        const int level = tibia::game::localPlayerLevel();
        const qint64 remaining = std::max<qint64>(0, experienceForLevel(level + 1) - experience);
        entry.valueString = number(experience);
        entry.additionalValueString = number(level);
        entry.progressBarValue = percent / 100.0;
        entry.progressBarColor = QColor(0xc0, 0x00, 0x00);
        const QString progress = percent <= 99.0 ? qtTrId("skill_progress_tooltip").arg(100.0 - percent, 0, 'f', 2)
                                                 : qtTrId("skill_progress_tooltip_almost_there");
        entry.tooltip = qtTrId("exp_to_levelup_tooltip").arg(number(remaining)) + QLatin1Char('\n') + progress;
        return entry;
    }

    qint64 value = 0;
    qint64 base = 0;
    qint64 loyaltyBase = 0;
    double percent = 0;
    if (type == SkillType::MagicLevel) {
        value = tibia::game::localPlayerMagicLevel();
        base = tibia::game::localPlayerBaseMagicLevel();
        loyaltyBase = tibia::game::localPlayerMagicLevelLoyalty();
        percent = tibia::game::localPlayerMagicLevelPercentHundredths() / 100.0;
    } else if (const auto skill = otcSkill(type)) {
        value = tibia::game::localPlayerSkillLevel(*skill);
        base = tibia::game::localPlayerSkillBaseLevel(*skill);
        loyaltyBase = tibia::game::localPlayerSkillLoyaltyLevel(*skill);
        percent = tibia::game::localPlayerSkillPercentHundredths(*skill) / 100.0;
    }
    // Servers that send no loyalty level report the base level there.
    if (loyaltyBase == 0)
        loyaltyBase = base;

    entry.valueString = number(value);
    entry.progressBarValue = percent / 100.0;
    entry.progressBarColor = QColor(0x00, 0xc0, 0x00);
    entry.tooltip = qtTrId("skill_progress_tooltip").arg(100.0 - percent, 0, 'f', 2);
    if (const QString detail = skillDetail(value, base, loyaltyBase); !detail.isEmpty())
        entry.tooltip = detail + QLatin1Char('\n') + entry.tooltip;
    using Color = SkillsWidgetEntry::ValueColor;
    if (value > loyaltyBase)
        entry.valueStringColor = Color::MODIFIER_POSITIVE;
    else if (value == loyaltyBase)
        entry.valueStringColor = Color::DEFAULT;
    else
        entry.valueStringColor = value > 0 ? Color::MODIFIER_NEGATIVE : Color::MODIFIER_ZERO;
    return entry;
}

namespace {

/// CIP / : "hh:mm" of minutes / seconds.
QString clockMinutes(const qint64 minutes)
{
    return QStringLiteral("%1:%2").arg(minutes / 60, 2, 10, QLatin1Char('0')).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

/// CIP .
QString hoursAndMinutes(const qint64 minutes) { return qtTrId("time_hours_and_minutes").arg(minutes / 60).arg(minutes % 60); }

using Color = SkillsWidgetEntry::ValueColor;

/// CIP style comparison: `zero` when the value is empty.
Color compareColor(const qint64 value, const qint64 reference, const bool zero)
{
    if (zero)
        return Color::MODIFIER_ZERO;
    if (value > reference)
        return Color::MODIFIER_POSITIVE;
    if (value < reference)
        return Color::MODIFIER_NEGATIVE;
    return Color::DEFAULT;
}

/// CIP : "%1%" of (base + low level + store boost + daily reward boost) x stamina.
void fillExperienceGain(SkillsWidgetEntry& entry)
{
    const auto rate = [&](const tibia::client::ExperienceRate_t type) {
        return tibia::game::localPlayerExperienceRate(type) / 100.0;
    };
    const double base = tibia::game::localPlayerExperienceRate(tibia::client::EXP_BASE) ? rate(tibia::client::EXP_BASE) : 1.0;
    const double lowLevel = rate(tibia::client::EXP_LOWLEVEL);
    const double storeBoost = rate(tibia::client::EXP_XPBOOST);
    const double dailyRewardBoost = rate(tibia::client::EXP_DAILYREWARD);
    const double stamina = tibia::game::localPlayerExperienceRate(tibia::client::EXP_STANINAMULTIPLIER)
                               ? rate(tibia::client::EXP_STANINAMULTIPLIER)
                               : 1.0;
    const double gain = (base + lowLevel + storeBoost + dailyRewardBoost) * stamina * 100.0;
    constexpr double defaultGain = 100.0;

    entry.valueString = QStringLiteral("%1%").arg(gain, 0, 'g', -1);
    if (gain > defaultGain)
        entry.valueStringColor = Color::MODIFIER_POSITIVE;
    else if (gain < defaultGain)
        entry.valueStringColor = gain > 0.0 ? Color::MODIFIER_NEGATIVE : Color::MODIFIER_ZERO;
    else
        entry.valueStringColor = Color::DEFAULT;

    QString tooltip = qtTrId("xp_gain_default_tooltip").arg(gain, 0, 'g', -1).arg(static_cast<int>(base * 100.0 + 0.5));
    if (lowLevel > 0.0)
        tooltip += qtTrId("xp_gain_lowlevel_tooltip").arg(static_cast<int>(lowLevel * 100.0 + 0.5));
    if (const qint64 boostSeconds = tibia::game::localPlayerStoreExpBoostTime(); boostSeconds > 0) {
        if (storeBoost != 0.0)
            tooltip += qtTrId("xp_gain_storeboost_running_tooltip")
                           .arg(static_cast<int>(storeBoost * 100.0 + 0.5))
                           .arg(clockMinutes((boostSeconds + 59) / 60));
        else
            tooltip += qtTrId("xp_gain_storeboost_paused_tooltip").arg(static_cast<int>(storeBoost * 100.0 + 0.5));
    }
    if (const qint64 boostSeconds = tibia::game::localPlayerDailyRewardExpBoostTime(); boostSeconds > 0) {
        if (dailyRewardBoost != 0.0)
            tooltip += qtTrId("xp_gain_dailyrewardboost_running_tooltip")
                           .arg(static_cast<int>(dailyRewardBoost * 100.0 + 0.5))
                           .arg(clockMinutes((boostSeconds + 59) / 60));
        else
            tooltip += qtTrId("xp_gain_dailyrewardboost_paused_tooltip")
                           .arg(static_cast<int>(dailyRewardBoost * 100.0 + 0.5));
    }
    if (stamina > 1.0)
        tooltip += qtTrId("xp_gain_hunting_bonus_tooltip")
                       .arg(stamina, 0, 'g', 2)
                       .arg(clockMinutes(tibia::game::localPlayerStamina() - 2340));
    else if (stamina < 1.0)
        tooltip += qtTrId("xp_gain_hunting_malus_tooltip").arg(stamina, 0, 'g', 2);
    entry.tooltip = tooltip;
}

}

SkillsWidgetEntry buildSkillsWidgetEntry(const SkillType type)
{
    switch (type) {
        case SkillType::Experience:
        case SkillType::MagicLevel:
        case SkillType::Shielding:
        case SkillType::Distance:
        case SkillType::Club:
        case SkillType::Sword:
        case SkillType::Axe:
        case SkillType::Fist:
        case SkillType::Fishing: {
            SkillsWidgetEntry entry = buildSkillEntry(type);
            entry.progressBarVisible = skillHasProgressBar(type);
            return entry;
        }
        default: break;
    }

    SkillsWidgetEntry entry;
    entry.caption = skillName(type);
    entry.progressBarVisible = skillHasProgressBar(type);
    if (!hasLocalPlayerSession())
        return entry;

    switch (type) {
        case SkillType::Level: {
            const double percent = levelPercentForDisplay();
            entry.valueString = number(tibia::game::localPlayerLevel());
            entry.progressBarValue = percent / 100.0;
            entry.progressBarColor = QColor(0xc0, 0x00, 0x00);
            entry.tooltip = qtTrId("skill_progress_tooltip").arg(100.0 - percent, 0, 'f', 2);
            break;
        }
        case SkillType::ExperienceGain:
            fillExperienceGain(entry);
            break;
        case SkillType::HitPoints:
            entry.valueString = number(static_cast<qint64>(tibia::game::localPlayerHealth()));
            entry.tooltip = qtTrId("x_of_y_foos_left_tooltips")
                                .arg(number(static_cast<qint64>(tibia::game::localPlayerHealth())))
                                .arg(number(static_cast<qint64>(tibia::game::localPlayerMaxHealth())))
                                .arg(qtTrId("hit_points"));
            break;
        case SkillType::Mana:
            entry.valueString = number(static_cast<qint64>(tibia::game::localPlayerMana()));
            entry.tooltip = qtTrId("x_of_y_foos_left_tooltips")
                                .arg(number(static_cast<qint64>(tibia::game::localPlayerMana())))
                                .arg(number(static_cast<qint64>(tibia::game::localPlayerMaxMana())))
                                .arg(qtTrId("mana"));
            break;
        case SkillType::Speed: {
            const qint64 speed = tibia::game::localPlayerSpeed();
            const qint64 base = tibia::game::localPlayerBaseSpeed();
            entry.valueString = number(speed);
            entry.valueStringColor = compareColor(speed, base, speed < base && speed == 0);
            entry.tooltip = skillDetail(speed, base, base);
            break;
        }
        case SkillType::Capacity: {
            const qint64 free = tibia::game::localPlayerFreeCapacity();
            const qint64 total = std::max<qint64>(tibia::game::localPlayerTotalCapacity(), free);
            const qint64 baseCap = tibia::game::localPlayerBaseCapacity();
            const qint64 base = baseCap ? baseCap : total;
            entry.valueString = number(free);
            entry.valueStringColor = compareColor(total, base, free == 0);
            entry.tooltip = qtTrId("x_of_y_foos_left_tooltips").arg(number(free)).arg(number(total)).arg(qtTrId("capacity"));
            if (const QString detail = skillDetail(total, base, base); !detail.isEmpty())
                entry.tooltip += QLatin1Char('\n') + detail;
            break;
        }
        case SkillType::Food: {
            const qint64 seconds = tibia::game::localPlayerRegenerationTime();
            entry.valueString = QStringLiteral("%1:%2")
                                    .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
                                    .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'));
            entry.tooltip = seconds > 0
                ? qtTrId("food_tooltip").arg(qtTrId("time_minutes_and_seconds").arg(seconds / 60).arg(seconds % 60))
                : qtTrId("food_hungry_tooltip");
            break;
        }
        case SkillType::SoulPoints: entry.valueString = number(tibia::game::localPlayerSoul()); break;
        case SkillType::Stamina: {
            const qint64 minutes = tibia::game::localPlayerStamina();
            entry.valueString = clockMinutes(minutes);
            entry.progressBarValue = minutes / 2520.0;
            const QString left = hoursAndMinutes(minutes);
            // CIP : >2340 premium, >840 regular, >0 malus, else none.
            if (minutes > 2340) {
                entry.tooltip = qtTrId("stamina_left_premium_tooltip").arg(left);
                entry.progressBarColor = QColor(0x00, 0xff, 0x00);
            } else if (minutes > 840) {
                entry.tooltip = qtTrId("stamina_left_tooltip").arg(left);
                entry.progressBarColor = QColor(0xc0, 0x60, 0x00);
            } else if (minutes > 0) {
                entry.tooltip = qtTrId("stamina_left_malus_tooltip").arg(left);
                entry.progressBarColor = QColor(0xc0, 0x60, 0x00);
            } else {
                entry.tooltip = qtTrId("no_stamina_left_tooltip");
                entry.progressBarColor = QColor(0xff, 0x00, 0x00);
            }
            break;
        }
        case SkillType::OfflineTraining: {
            const qint64 minutes = tibia::game::localPlayerOfflineTrainingTime();
            entry.valueString = clockMinutes(minutes);
            entry.progressBarValue = minutes / 720.0;
            entry.progressBarColor = QColor(0xc0, 0x00, 0x00);
            entry.tooltip = qtTrId("offline_training_left_tooltip").arg(hoursAndMinutes(minutes));
            break;
        }
        default: break;
    }
    return entry;
}

}
