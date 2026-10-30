#include "combatstatsqmldata.h"

#include <QLocale>
#include <QRegularExpression>

#include <cmath>

namespace tibia::cyclopedia {

namespace {

enum Role { NameRole = Qt::UserRole + 1, ValueRole, TooltipRole };

const QLocale& englishLocale()
{
    static const QLocale locale(QLocale::English);
    return locale;
}

/// CIP FUN_1009601d0: the long and the short caption as one QML multi string.
QString multiString(const char* longId, const char* shortId)
{
    return qtTrId(longId) + QChar(0x9c) + qtTrId(shortId);
}

TQmlStatWithSources* makeStat(const QString& caption, const QString& tooltip, const QString& total, const bool visible,
                              QObject* parent, const TQmlStatWithSources::Icon& icon = {},
                              const QString& styleType = QStringLiteral("Dialog"))
{
    return new TQmlStatWithSources(caption, tooltip, total, visible, {}, styleType, icon, parent);
}

/// The leech / onslaught / momentum pattern: "<tooltip>".arg(+x), total "+x%",
/// shown only when positive.
TQmlStatWithSources* makeFractionStat(const char* captionId, const char* tooltipId, const double value, QObject* parent)
{
    return makeStat(qtTrId(captionId), qtTrId(tooltipId).arg(formatFraction(value, false, true)),
                    formatFraction(value, true, true), value > 0.0, parent);
}

TQmlStatWithSources::Icon damageTypeIconOf(const int damageType)
{
    return { damageTypeIcon(damageType), damageTypeName(damageType) };
}

}

TStatSourcesListModel::TStatSourcesListModel(std::vector<StatSource> sources, QObject* parent)
    : cip::tibia::cyclopedia::TStatSourcesListModel(parent)
    , m_sources(std::move(sources))
{}

int TStatSourcesListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sources.size());
}

QVariant TStatSourcesListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const StatSource& source = m_sources[static_cast<size_t>(index.row())];
    switch (role) {
        case NameRole: return source.name;
        case ValueRole: return source.value;
        case TooltipRole: return source.tooltip;
        default: return {};
    }
}

QHash<int, QByteArray> TStatSourcesListModel::roleNames() const
{
    return { { NameRole, "name" }, { ValueRole, "value" }, { TooltipRole, "tooltip" } };
}

TQmlStatWithSources::TQmlStatWithSources(const QString& caption, const QString& tooltip, const QString& total,
                                         const bool visible, std::vector<StatSource> sources,
                                         const QString& styleType, const Icon& icon, QObject* parent)
    : cip::tibia::cyclopedia::TQmlStatWithSources(parent)
{
    m_caption = caption;
    m_tooltip = tooltip;
    m_total = total;
    m_visible = visible;
    m_styleType = styleType;
    m_iconSource = icon.source;
    m_iconTooltip = icon.tooltip;
    m_sources = new TStatSourcesListModel(std::move(sources), this);
}

QString formatDecimal(const double value, const int minDecimals, const int maxDecimals)
{
    const int minimum = maxDecimals < minDecimals ? 0 : std::max(minDecimals, 0);
    QString text = englishLocale().toString(value, 'f', maxDecimals);
    const qsizetype dot = text.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        if (minimum > 0)
            text += QLatin1Char('.') + QString(minimum, QLatin1Char('0'));
        return text;
    }
    const int decimals = static_cast<int>(text.size() - dot - 1);
    if (decimals >= minimum) {
        static const QRegularExpression trailingZeros(QStringLiteral("0+$"));
        static const QRegularExpression trailingDot(QStringLiteral("\\.$"));
        text.remove(trailingZeros);
        text.remove(trailingDot);
    } else {
        text += QString(minimum - decimals, QLatin1Char('0'));
    }
    return text;
}

QString formatFraction(const double value, const bool showPercent, const bool showSign)
{
    const QString sign = showSign && value >= 0.0 ? QStringLiteral("+") : QString();
    const QString pattern = showPercent ? QStringLiteral("%1%2%") : QStringLiteral("%1%2");
    return pattern.arg(sign).arg(formatDecimal(value * 100.0, 0, 2));
}

QString formatInteger(const qint64 value, const bool showSign)
{
    return (showSign ? QStringLiteral("+") : QString()) + englishLocale().toString(value);
}

QString damageTypeName(const int damageType)
{
    switch (damageType) {
        case -1: return {};
        case 1: return qtTrId("damagetype_fire");
        case 2: return qtTrId("damagetype_earth");
        case 3: return qtTrId("damagetype_energy");
        case 4: return qtTrId("damagetype_ice");
        case 5: return qtTrId("damagetype_holy");
        case 6: return qtTrId("damagetype_death");
        case 7: return qtTrId("damagetype_healing");
        case 8: return qtTrId("damagetype_drowning");
        case 9: return qtTrId("damagetype_lifeleech");
        case 10: return qtTrId("damagetype_manadrain");
        case 11: return qtTrId("damagetype_agony");
        default: return qtTrId("damagetype_physical");
    }
}

QUrl damageTypeIcon(const int damageType)
{
    const QString name = damageTypeName(damageType).toLower().replace(QLatin1Char(' '), QString());
    return QUrl(QStringLiteral("/images/monster-icon-%1-resist.png").arg(name));
}

TOffenceStatsQmlData::TOffenceStatsQmlData(const CombatStats& stats, QObject* parent)
    : cip::tibia::cyclopedia::TOffenceStatsQmlData(parent)
{
    const QString critTooltip = qtTrId("crit_damage_tooltip")
                                    .arg(formatFraction(stats.critChance, false, true))
                                    .arg(formatFraction(stats.critExtraDamage, false, true));
    m_critChance = makeStat(qtTrId("skill_chance"), critTooltip, formatFraction(stats.critChance, true, true),
                            stats.critChance > 0.0, this);
    m_critExtraDamage = makeStat(qtTrId("critical_extra_damage"), critTooltip,
                                 formatFraction(stats.critExtraDamage, true, true), stats.critExtraDamage > 0.0, this);

    m_flatDamageHealing = makeStat(multiString("charinfo_flat_damage_healing", "charinfo_flat_damage_healing_short"),
                                   qtTrId("charinfo_flat_damage_healing_tooltip"),
                                   formatInteger(stats.flatDamageHealing, false), true, this);
    m_attackValue = makeStat(qtTrId("charinfo_attack_value"), qtTrId("charinfo_attack_value_tooltip"),
                             formatInteger(stats.attackValue, false), true, this,
                             damageTypeIconOf(stats.attackDamageType));
    m_specialAttackConversion =
        makeStat(qtTrId("charinfo_special_attack"),
                 qtTrId("charinfo_special_attack_tooltip")
                     .arg(formatFraction(stats.convertedDamage, true, true))
                     .arg(damageTypeName(stats.convertedDamageType)),
                 formatFraction(stats.convertedDamage, true, true), stats.convertedDamage > 0.0, this,
                 damageTypeIconOf(stats.convertedDamageType));

    m_lifeLeech = makeFractionStat("life_leech", "life_leech_tooltip", stats.lifeLeech, this);
    m_manaLeech = makeFractionStat("mana_leech", "mana_leech_tooltip", stats.manaLeech, this);
    m_onslaught = makeFractionStat("onslaught_caption", "onslaught_tooltip", stats.onslaught, this);
}

TDefenceStatsQmlData::TDefenceStatsQmlData(const CombatStats& stats, QObject* parent)
    : cip::tibia::cyclopedia::TDefenceStatsQmlData(parent)
{
    const auto integer = [](const int value) { return englishLocale().toString(value); };

    m_defence = makeStat(qtTrId("charinfo_defence_value"), qtTrId("charinfo_defence_value_tooltip"),
                         integer(stats.defence), true, this);
    m_armorValue = makeStat(qtTrId("charinfo_armor_value"), qtTrId("charinfo_armor_value_tooltip"),
                            integer(stats.armor), true, this);
    m_mantraValue = makeStat(qtTrId("charinfo_mantra_value"), qtTrId("charinfo_mantra_value_tooltip"),
                             integer(stats.mantra), stats.mantra > 0, this);
    m_mitigation = makeStat(qtTrId("charinfo_mitigation_percent"), qtTrId("charinfo_mitigation_percent_tooltip"),
                            formatFraction(stats.mitigation, true, true), true, this);
    m_magicShield = makeStat(qtTrId("magic_shield_capacity_bonus_caption"), QString(), integer(0), false, this);
    m_dodge = makeStat(qtTrId("dodge"), qtTrId("dodge_tooltip").arg(formatFraction(stats.dodge, false, true)),
                       formatFraction(stats.dodge, true, true), stats.dodge > 0.0, this);
    m_damageReflection =
        makeStat(qtTrId("damage_reflection_amount"), qtTrId("damage_reflection_tooltip").arg(integer(stats.damageReflection)),
                 integer(stats.damageReflection), stats.damageReflection > 0, this);

    for (const auto& [damageType, value] : stats.damageReductions) {
        const QString name = damageTypeName(damageType);
        const QString tooltip = qtTrId(value < 0.0 ? "charinfo_damage_increase_tooltip" : "charinfo_damage_reduction_tooltip")
                                    .arg(name)
                                    .arg(formatFraction(std::fabs(value), false, true))
                                    .arg(qtTrId("charinfo_damage_reduction_header_tooltip"));
        QString style = QStringLiteral("Dialog");
        if (value > 0.0)
            style = QStringLiteral("ModifierPositive");
        else if (value < 0.0)
            style = QStringLiteral("ModifierNegative");
        m_damageReductions.append(makeStat(name, tooltip, formatFraction(value, true, true), true, this,
                                           { damageTypeIcon(damageType), QString() }, style));
    }
}

TMiscStatsQmlData::TMiscStatsQmlData(const CombatStats& stats, QObject* parent)
    : cip::tibia::cyclopedia::TMiscStatsQmlData(parent)
{
    m_momentum = makeFractionStat("momentum_caption", "momentum_tooltip", stats.momentum, this);
    m_transcendence = makeFractionStat("transcendence_caption", "transcendence_tooltip", stats.transcendence, this);
    m_amplification = makeFractionStat("amplification_caption", "amplification_tooltip", stats.amplification, this);
}

}
