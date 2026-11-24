#include "combatstatsqmldata.h"

#include "proficiency/proficiencyperks.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/thingsession_forward.h>

#include <QLocale>
#include <QRegularExpression>

#include <array>
#include <cmath>
#include <functional>
#include <optional>

namespace tibia::cyclopedia {

namespace {

enum Role { NameRole = Qt::UserRole + 1, ValueRole, TooltipRole };

const QLocale& englishLocale()
{
    static const QLocale locale(QLocale::English);
    return locale;
}

/// CIP : the long and the short caption as one QML multi string.
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

namespace {

using game::IOffenceStatsProvider;

/// CIP: the name of a stat source.
QString sourceName(const int source)
{
    switch (source) {
        case 0: return qtTrId("charinfo_source_flat_damage");
        case 1: return qtTrId("charinfo_source_level");
        case 2: return qtTrId("charinfo_source_equipment");
        case 3: return qtTrId("charinfo_source_amplification");
        case 4: return qtTrId("charinfo_source_imbuement");
        case 5: return qtTrId("charinfo_source_skill");
        case 6: return qtTrId("charinfo_source_skillwheel");
        case 7: return qtTrId("charinfo_source_concoction");
        case 8: return qtTrId("charinfo_source_event_bonus");
        case 9: return qtTrId("charinfo_source_combat_tactics");
        case 10: return qtTrId("charinfo_source_defence");
        default: return QString();
    }
}

/// CIP: the tooltip of a stat source.
QString sourceTooltip(const int source)
{
    switch (source) {
        case 0: return qtTrId("charinfo_flat_damage_healing_tooltip");
        case 2: return qtTrId("charinfo_source_tooltip_equipment");
        case 6: return qtTrId("charinfo_source_tooltip_skillwheel");
        case 9: return qtTrId("charinfo_source_tooltip_combat_tactics");
        default: return QString();
    }
}

/// CIP: the name of skill type 4 (magic level) .. 16 (fishing).
QString skillSourceName(const int skill)
{
    static constexpr std::array<const char*, 13> Names{
        "charinfo_skill_magic",    "charinfo_source_skill",   "charinfo_source_skill", "charinfo_source_skill",
        "charinfo_source_skill",   "charinfo_source_skill",   "charinfo_skill_shielding", "charinfo_skill_distance",
        "charinfo_skill_club",     "charinfo_skill_sword",    "charinfo_skill_axe",    "charinfo_skill_fist",
        "charinfo_skill_fishing",
    };
    const unsigned index = static_cast<unsigned>(skill - 4);
    return qtTrId(index < Names.size() ? Names[index] : "charinfo_source_skill");
}

/// CIP: one row per source that has a part, in source order. The skill
/// source is named after `skill` when one is given.
template <class T>
std::vector<StatSource> statSources(const std::function<std::optional<T>(ECombatStatSource)>& value,
                                    const std::function<QString(T)>& format, const std::optional<int> skill = {})
{
    std::vector<StatSource> sources;
    for (int source = 0; source < CombatStatSourceCount; ++source) {
        const std::optional<T> part = value(static_cast<ECombatStatSource>(source));
        if (!part)
            continue;
        const QString name = source == 5 && skill ? skillSourceName(*skill) : sourceName(source);
        sources.push_back({ name, format(*part), sourceTooltip(source) });
    }
    return sources;
}

QString signedPercent(const double value) { return formatFraction(value, true, true); }
QString unsignedInteger(const int value) { return formatInteger(value, false); }

TQmlStatWithSources* makeSourcedStat(const QString& caption, const QString& tooltip, const QString& total, const bool visible,
                                     std::vector<StatSource> sources, QObject* parent,
                                     const TQmlStatWithSources::Icon& icon = {})
{
    return new TQmlStatWithSources(caption, tooltip, total, visible, std::move(sources), QStringLiteral("Dialog"), icon,
                                   parent);
}

/// The crit / leech / onslaught pattern: tooltip with the signed value,
/// total "+x%", shown when positive, rows from the sources.
TQmlStatWithSources* makePercentStat(const char* captionId, const QString& tooltip, const double value,
                                     const std::function<std::optional<double>(ECombatStatSource)>& source,
                                     QObject* parent)
{
    return makeSourcedStat(qtTrId(captionId), tooltip, formatFraction(value, true, true), value > 0.0,
                           statSources<double>(source, signedPercent), parent);
}

/// The life / mana on hit / kill pattern: an integer, shown when not zero.
TQmlStatWithSources* makeIntegerStat(const char* captionId, const char* tooltipId, const int value, QObject* parent)
{
    return makeSourcedStat(qtTrId(captionId), qtTrId(tooltipId), formatInteger(value, false), value != 0, {}, parent);
}

/// The alpha / omega strike, armor penetration and cleave pattern.
TQmlStatWithSources* makeBonusStat(const char* captionId, const char* tooltipId, const double value, QObject* parent)
{
    return makeSourcedStat(qtTrId(captionId), qtTrId(tooltipId).arg(formatFraction(value, false, true)),
                           formatFraction(value, true, true), value > 0.0, {}, parent);
}

QObject* makeSkillBonusStat(const IOffenceStatsProvider::SkillBonuses& bonuses, const char* captionId,
                            const char* tooltipId, const char* sourceTooltipId, QObject* parent)
{
    std::vector<StatSource> sources;
    for (const auto& [skill, bonus] : bonuses) {
        const QString tooltip = qtTrId(sourceTooltipId).arg(bonus.first * 100.0).arg(skillSourceName(skill));
        sources.push_back({ skillSourceName(skill), formatDecimal(bonus.second, 0, 2), tooltip });
    }
    double sum = 0.0;
    for (const auto& [skill, bonus] : bonuses)
        sum += bonus.second;
    const bool visible = !sources.empty();
    return makeSourcedStat(qtTrId(captionId), qtTrId(tooltipId), englishLocale().toString(static_cast<qint64>(std::lround(sum))),
                           visible, std::move(sources), parent);
}

/// The element spell / offensive rune / auto attack rows of the special crits.
std::vector<StatSource> specialCritSources(const std::map<int, double>& elements, const double runes,
                                           const double autoAttacks)
{
    std::vector<StatSource> sources;
    for (const auto& [damageType, value] : elements) {
        sources.push_back({ qtTrId("charinfo_source_element_spells").arg(damageTypeName(damageType)),
                            formatFraction(value, true, true), QString() });
    }
    if (runes > 0.0)
        sources.push_back({ qtTrId("charinfo_source_offensive_runes"), formatFraction(runes, true, true), QString() });
    if (autoAttacks > 0.0)
        sources.push_back({ qtTrId("charinfo_source_auto_attacks"), formatFraction(autoAttacks, true, true), QString() });
    return sources;
}

/// The skills widget's PlayerSkills stats: totals only, no source parts.
class CombatStatsOffenceProvider final : public IOffenceStatsProvider
{
public:
    explicit CombatStatsOffenceProvider(const CombatStats& stats)
        : m_stats(stats)
    {}

    double critChance() const override { return m_stats.critChance; }
    std::optional<double> critChance(ECombatStatSource) const override { return std::nullopt; }
    double critExtraDamage() const override { return m_stats.critExtraDamage; }
    std::optional<double> critExtraDamage(ECombatStatSource) const override { return std::nullopt; }
    double lifeLeech() const override { return m_stats.lifeLeech; }
    std::optional<double> lifeLeech(ECombatStatSource) const override { return std::nullopt; }
    double manaLeech() const override { return m_stats.manaLeech; }
    std::optional<double> manaLeech(ECombatStatSource) const override { return std::nullopt; }
    int flatDamageHealing() const override { return m_stats.flatDamageHealing; }
    std::optional<int> flatDamageHealing(ECombatStatSource) const override { return std::nullopt; }
    int attackValue() const override { return m_stats.attackValue; }
    std::optional<int> attackValue(ECombatStatSource) const override { return std::nullopt; }
    int attackDamageType() const override { return m_stats.attackDamageType; }
    std::optional<int> attackSkill() const override { return std::nullopt; }
    double convertedDamage() const override { return m_stats.convertedDamage; }
    int convertedDamageType() const override { return m_stats.convertedDamageType; }
    double cleave() const override { return 0.0; }
    int perfectShotDamage(int) const override { return 0; }
    double alphaStrike() const override { return 0.0; }
    double omegaStrike() const override { return 0.0; }
    double armorPenetration() const override { return 0.0; }
    std::map<int, double> elementalPierce() const override { return {}; }
    double onslaught() const override { return m_stats.onslaught; }
    std::optional<double> onslaught(ECombatStatSource) const override { return std::nullopt; }
    std::vector<std::pair<int, double>> distanceAccuracy() const override { return {}; }
    double damageAgainstPowerfulFoes() const override { return 0.0; }
    std::map<std::string, double> specificTargetBonuses() const override { return {}; }
    std::map<int, double> elementSpellCritChances() const override { return {}; }
    double offensiveRuneCritChance() const override { return 0.0; }
    double autoAttackCritChance() const override { return 0.0; }
    std::map<int, double> elementSpellCritDamages() const override { return {}; }
    double offensiveRuneCritDamage() const override { return 0.0; }
    double autoAttackCritDamage() const override { return 0.0; }
    int lifeOnHit() const override { return 0; }
    int manaOnHit() const override { return 0; }
    int lifeOnKill() const override { return 0; }
    int manaOnKill() const override { return 0; }
    SkillBonuses autoAttackBonusFromSkills() const override { return {}; }
    SkillBonuses spellBonusDamageFromSkills() const override { return {}; }
    SkillBonuses spellBonusHealingFromSkills() const override { return {}; }

private:
    CombatStats m_stats;
};

}

TOffenceStatsQmlData::TOffenceStatsQmlData(const CombatStats& stats, QObject* parent)
    : TOffenceStatsQmlData(std::make_shared<CombatStatsOffenceProvider>(stats), parent)
{}

TOffenceStatsQmlData::TOffenceStatsQmlData(std::shared_ptr<game::IOffenceStatsProvider> provider, QObject* parent)
    : cip::tibia::cyclopedia::TOffenceStatsQmlData(parent)
    , m_provider(std::move(provider))
{
    const IOffenceStatsProvider& p = *m_provider;
    const auto critTooltip = [&p] {
        return qtTrId("crit_damage_tooltip")
            .arg(formatFraction(p.critChance(), false, true))
            .arg(formatFraction(p.critExtraDamage(), false, true));
    };
    const std::shared_ptr<IOffenceStatsProvider> shared = m_provider;

    m_critChance = makePercentStat("skill_chance", critTooltip(), p.critChance(),
                                   [shared](ECombatStatSource s) { return shared->critChance(s); }, this);
    m_specialCritChances = makeSpecialCritChances();
    m_critExtraDamage = makePercentStat("critical_extra_damage", critTooltip(), p.critExtraDamage(),
                                        [shared](ECombatStatSource s) { return shared->critExtraDamage(s); }, this);
    m_specialCritDamages = makeSpecialCritDamages();
    m_lifeLeech = makePercentStat("life_leech", qtTrId("life_leech_tooltip").arg(formatFraction(p.lifeLeech(), false, true)),
                                  p.lifeLeech(), [shared](ECombatStatSource s) { return shared->lifeLeech(s); }, this);
    m_manaLeech = makePercentStat("mana_leech", qtTrId("mana_leech_tooltip").arg(formatFraction(p.manaLeech(), false, true)),
                                  p.manaLeech(), [shared](ECombatStatSource s) { return shared->manaLeech(s); }, this);
    m_lifeOnHit = makeIntegerStat("charinfo_life_on_hit", "charinfo_life_on_hit_tooltip", p.lifeOnHit(), this);
    m_manaOnHit = makeIntegerStat("charinfo_mana_on_hit", "charinfo_mana_on_hit_tooltip", p.manaOnHit(), this);
    m_lifeOnKill = makeIntegerStat("charinfo_life_on_kill", "charinfo_life_on_kill_tooltip", p.lifeOnKill(), this);
    m_manaOnKill = makeIntegerStat("charinfo_mana_on_kill", "charinfo_mana_on_kill_tooltip", p.manaOnKill(), this);
    m_onslaught = makePercentStat("onslaught_caption", qtTrId("onslaught_tooltip").arg(formatFraction(p.onslaught(), false, true)),
                                  p.onslaught(), [shared](ECombatStatSource s) { return shared->onslaught(s); }, this);

    // Specific targets by name, then the powerful foes.
    std::vector<StatSource> targets;
    for (const auto& [name, value] : p.specificTargetBonuses())
        targets.push_back({ QString::fromStdString(name), formatFraction(value, true, true), QString() });
    if (p.damageAgainstPowerfulFoes() > 0.0) {
        targets.push_back({ qtTrId("charinfo_damage_bonus_against_special_monsters"),
                            formatFraction(p.damageAgainstPowerfulFoes(), true, true), QString() });
    }
    const bool hasTargets = !targets.empty();
    m_specificTargetAttackBonus =
        makeSourcedStat(qtTrId("charinfo_damage_bonus_against_specific_targets"),
                        qtTrId("charinfo_damage_bonus_against_specific_targets_tooltip"), QString(), hasTargets,
                        std::move(targets), this);

    m_cleave = makeBonusStat("cleave_percent", "cleave_percent_tooltip", p.cleave(), this);

    m_flatDamageHealing = makeSourcedStat(
        multiString("charinfo_flat_damage_healing", "charinfo_flat_damage_healing_short"),
        qtTrId("charinfo_flat_damage_healing_tooltip"), formatInteger(p.flatDamageHealing(), false), true,
        statSources<int>([shared](ECombatStatSource s) { return shared->flatDamageHealing(s); }, unsignedInteger), this);

    m_attackValue = makeSourcedStat(
        qtTrId("charinfo_attack_value"), qtTrId("charinfo_attack_value_tooltip"), formatInteger(p.attackValue(), false), true,
        statSources<int>([shared](ECombatStatSource s) { return shared->attackValue(s); }, unsignedInteger, p.attackSkill()),
        this, damageTypeIconOf(p.attackDamageType()));

    const QString converted = formatFraction(p.convertedDamage(), true, true);
    m_specialAttackConversion =
        makeSourcedStat(qtTrId("charinfo_special_attack"),
                        qtTrId("charinfo_special_attack_tooltip").arg(converted).arg(damageTypeName(p.convertedDamageType())),
                        converted, p.convertedDamage() > 0.0, {}, this, damageTypeIconOf(p.convertedDamageType()));

    // Perfect shot: one row per range with damage.
    std::vector<StatSource> perfectShot;
    for (int range = 1; range <= 7; ++range) {
        const int damage = p.perfectShotDamage(range);
        if (damage <= 0)
            continue;
        perfectShot.push_back({ qtTrId("charinfo_perfect_shot_range").arg(range), formatInteger(damage, true),
                                qtTrId("perfect_shot_extra_damage_tooltip").arg(formatInteger(damage, false)).arg(range) });
    }
    const bool hasPerfectShot = !perfectShot.empty();
    m_perfectShot = makeSourcedStat(qtTrId("perfect_shot_extra_damage_caption"), QString(), QString(), hasPerfectShot,
                                    std::move(perfectShot), this);

    m_alphaStrike = makeBonusStat("charinfo_damage_bonus_against_95_percent_HP",
                                  "charinfo_damage_bonus_against_95_percent_HP_tooltip", p.alphaStrike(), this);
    m_omegaStrike = makeBonusStat("charinfo_damage_bonus_against_30_percent_HP",
                                  "charinfo_damage_bonus_against_30_percent_HP_tooltip", p.omegaStrike(), this);
    m_armorPenetration = makeBonusStat("charinfo_armor_penetration", "charinfo_armor_penetration_tooltip",
                                       p.armorPenetration(), this);
    m_elementalPierce = makeElementalPierce();

    // Distance accuracy: consecutive ranges with the same chance form one row.
    struct RangeGroup
    {
        int from;
        int to;
        double chance;
    };
    const std::vector<std::pair<int, double>> accuracy = p.distanceAccuracy();
    std::vector<RangeGroup> groups;
    for (const auto& [range, chance] : accuracy) {
        if (!groups.empty() && range == groups.back().to + 1 && chance == groups.back().chance)
            groups.back().to = range;
        else
            groups.push_back({ range, range, chance });
    }
    std::vector<StatSource> ranges;
    for (const RangeGroup& group : groups) {
        const QString name = group.from == group.to
                                 ? qtTrId("charinfo_perfect_shot_range").arg(group.from)
                                 : qtTrId("charinfo_perfect_shot_range_from_to_range").arg(group.from).arg(group.to);
        ranges.push_back({ name, formatFraction(group.chance, true, false), QString() });
    }
    const QString accuracyTotal = groups.size() == 1 ? formatFraction(groups.front().chance, true, false) : QString();
    m_distanceFightingAccuracy = makeSourcedStat(qtTrId("charinfo_distance_accuracy"),
                                                 qtTrId("charminfo_distance_accuracy_tooltip"), accuracyTotal,
                                                 !accuracy.empty(), std::move(ranges), this);

    m_autoAttackBonusFromSkills = makeAutoAttackBonusFromSkills();
    m_spellBonusDamageFromSkills = makeSpellBonusDamageFromSkills();
    m_spellBonusHealingFromSkills = makeSpellBonusHealingFromSkills();
}

QObject* TOffenceStatsQmlData::makeSpecialCritChances()
{
    std::vector<StatSource> sources = specialCritSources(m_provider->elementSpellCritChances(),
                                                         m_provider->offensiveRuneCritChance(),
                                                         m_provider->autoAttackCritChance());
    const bool visible = !sources.empty();
    return makeSourcedStat(qtTrId("charinfo_special_crit_chance"), qtTrId("charinfo_special_crit_chance_tooltip"),
                           QString(), visible, std::move(sources), this);
}

QObject* TOffenceStatsQmlData::makeSpecialCritDamages()
{
    std::vector<StatSource> sources = specialCritSources(m_provider->elementSpellCritDamages(),
                                                         m_provider->offensiveRuneCritDamage(),
                                                         m_provider->autoAttackCritDamage());
    const bool visible = !sources.empty();
    return makeSourcedStat(qtTrId("charinfo_special_crit_damage"), qtTrId("charinfo_special_crit_damage_tooltip"),
                           QString(), visible, std::move(sources), this);
}

QObject* TOffenceStatsQmlData::makeElementalPierce()
{
    std::vector<StatSource> sources;
    for (const auto& [damageType, value] : m_provider->elementalPierce())
        sources.push_back({ damageTypeName(damageType), formatFraction(value, true, true), QString() });
    const bool visible = !sources.empty();
    return makeSourcedStat(qtTrId("charinfo_elemental_pierce"), qtTrId("charinfo_elemental_pierce_tooltip"), QString(),
                           visible, std::move(sources), this);
}

QObject* TOffenceStatsQmlData::makeAutoAttackBonusFromSkills()
{
    return makeSkillBonusStat(m_provider->autoAttackBonusFromSkills(), "charinfo_auto_attack_extra_damage",
                              "charinfo_auto_attack_extra_damage_tooltip",
                              "charinfo_auto_attack_extra_damage_source_tooltip", this);
}

QObject* TOffenceStatsQmlData::makeSpellBonusDamageFromSkills()
{
    return makeSkillBonusStat(m_provider->spellBonusDamageFromSkills(), "charinfo_spell_extra_damage",
                              "charinfo_spell_extra_damage_tooltip", "charinfo_spell_extra_damage_source_tooltip", this);
}

QObject* TOffenceStatsQmlData::makeSpellBonusHealingFromSkills()
{
    return makeSkillBonusStat(m_provider->spellBonusHealingFromSkills(), "charinfo_spell_extra_healing",
                              "charinfo_spell_extra_healing_tooltip", "charinfo_spell_extra_healing_source_tooltip",
                              this);
}

namespace {

using game::IDefenceStatsProvider;

/// CIP: a multiplying part, "* x%".
QString factorText(const double value)
{
    return QStringLiteral("* %1%").arg(formatDecimal(value * 100.0, 0, 2));
}

/// The skills widget's PlayerSkills stats: totals only, no source parts.
class CombatStatsDefenceProvider final : public IDefenceStatsProvider
{
public:
    explicit CombatStatsDefenceProvider(const CombatStats& stats)
        : m_stats(stats)
    {}

    int armor() const override { return m_stats.armor; }
    int mantra() const override { return m_stats.mantra; }
    int defence() const override { return m_stats.defence; }
    std::optional<int> defence(ECombatStatSource) const override { return std::nullopt; }
    std::optional<int> defenceSkill() const override { return std::nullopt; }
    double mitigation() const override { return m_stats.mitigation; }
    std::optional<double> mitigation(ECombatStatSource) const override { return std::nullopt; }
    std::optional<int> mitigationSkill() const override { return std::nullopt; }
    std::vector<ElementalResistance> elementalResistances() const override
    {
        std::vector<ElementalResistance> result;
        for (const auto& [damageType, value] : m_stats.damageReductions)
            result.push_back({ damageType, std::nullopt, value });
        return result;
    }
    std::vector<ElementalResistance> elementalResistanceSources(int) const override { return {}; }
    uint32_t magicShieldCapacity() const override { return 0; }
    uint32_t magicShieldCapacityFlat() const override { return 0; }
    double magicShieldCapacityPercent() const override { return 1.0; }
    int damageReflection() const override { return m_stats.damageReflection; }
    double dodge() const override { return m_stats.dodge; }
    std::optional<double> dodge(ECombatStatSource) const override { return std::nullopt; }

private:
    CombatStats m_stats;
};

}

TDefenceStatsQmlData::TDefenceStatsQmlData(const CombatStats& stats, QObject* parent)
    : TDefenceStatsQmlData(std::make_shared<CombatStatsDefenceProvider>(stats), parent)
{}

TDefenceStatsQmlData::TDefenceStatsQmlData(std::shared_ptr<game::IDefenceStatsProvider> provider, QObject* parent)
    : cip::tibia::cyclopedia::TDefenceStatsQmlData(parent)
    , m_provider(std::move(provider))
{
    const std::shared_ptr<IDefenceStatsProvider> shared = m_provider;
    const IDefenceStatsProvider& p = *m_provider;

    m_defence = makeSourcedStat(qtTrId("charinfo_defence_value"), qtTrId("charinfo_defence_value_tooltip"),
                                formatInteger(p.defence(), false), true,
                                statSources<int>([shared](ECombatStatSource s) { return shared->defence(s); }, unsignedInteger,
                                                 p.defenceSkill()),
                                this);
    m_armorValue = makeSourcedStat(qtTrId("charinfo_armor_value"), qtTrId("charinfo_armor_value_tooltip"),
                                   formatInteger(p.armor(), false), true, {}, this);
    m_mantraValue = makeSourcedStat(qtTrId("charinfo_mantra_value"), qtTrId("charinfo_mantra_value_tooltip"),
                                    formatInteger(p.mantra(), false), p.mantra() > 0, {}, this);

    // The defence and skill parts of the mitigation add, the others multiply
    // (a factor of 1 is no part).
    std::vector<StatSource> mitigationSources;
    for (const ECombatStatSource source : { ECombatStatSource::Defence, ECombatStatSource::Skill, ECombatStatSource::Equipment,
                                            ECombatStatSource::SkillWheel, ECombatStatSource::CombatTactics }) {
        const std::optional<double> part = p.mitigation(source);
        if (!part)
            continue;
        const int index = static_cast<int>(source);
        QString name;
        QString value;
        if (source == ECombatStatSource::Defence || source == ECombatStatSource::Skill) {
            value = formatFraction(*part, true, true);
            const std::optional<int> skill = source == ECombatStatSource::Skill ? p.mitigationSkill() : std::nullopt;
            name = skill ? skillSourceName(*skill) : sourceName(index);
        } else {
            if (*part == 1.0)
                continue;
            value = factorText(*part);
            name = sourceName(index);
        }
        mitigationSources.push_back({ name, value, sourceTooltip(index) });
    }
    m_mitigation = makeSourcedStat(qtTrId("charinfo_mitigation_percent"), qtTrId("charinfo_mitigation_percent_tooltip"),
                                   formatFraction(p.mitigation(), true, true), true, std::move(mitigationSources), this);

    std::vector<StatSource> shieldSources;
    if (p.magicShieldCapacityFlat() != 0)
        shieldSources.push_back({ qtTrId("magic_shield_capacity_flat_bonus"),
                                  englishLocale().toString(static_cast<qint64>(p.magicShieldCapacityFlat())), QString() });
    if (p.magicShieldCapacityPercent() != 1.0)
        shieldSources.push_back({ qtTrId("magic_shield_capacity_percent_bonus"), factorText(p.magicShieldCapacityPercent()), QString() });
    QString shieldTooltip;
    if (!shieldSources.empty()) {
        shieldTooltip = qtTrId("magic_shield_capacity_tooltip")
                            .arg(static_cast<qint64>(p.magicShieldCapacityFlat()))
                            .arg(formatFraction(p.magicShieldCapacityPercent(), false, true));
    }
    m_magicShield = makeSourcedStat(qtTrId("magic_shield_capacity_bonus_caption"), shieldTooltip,
                                    englishLocale().toString(static_cast<qint64>(p.magicShieldCapacity())), p.magicShieldCapacity() > 0,
                                    std::move(shieldSources), this);

    m_dodge = makeSourcedStat(qtTrId("dodge"), qtTrId("dodge_tooltip").arg(formatFraction(p.dodge(), false, true)),
                              formatFraction(p.dodge(), true, true), p.dodge() > 0.0,
                              statSources<double>([shared](ECombatStatSource s) { return shared->dodge(s); }, signedPercent), this);
    m_damageReflection = makeSourcedStat(qtTrId("damage_reflection_amount"),
                                         qtTrId("damage_reflection_tooltip").arg(formatInteger(p.damageReflection(), false)),
                                         formatInteger(p.damageReflection(), false), p.damageReflection() > 0, {}, this);

    for (const IDefenceStatsProvider::ElementalResistance& resistance : p.elementalResistances()) {
        std::vector<StatSource> sources;
        for (const IDefenceStatsProvider::ElementalResistance& part : p.elementalResistanceSources(resistance.damageType)) {
            const int source = part.source ? static_cast<int>(*part.source) : -1;
            sources.push_back({ sourceName(source), formatFraction(part.value, true, true), sourceTooltip(source) });
        }
        const QString name = damageTypeName(resistance.damageType);
        const double value = resistance.value;
        const QString tooltip = qtTrId(value < 0.0 ? "charinfo_damage_increase_tooltip" : "charinfo_damage_reduction_tooltip")
                                    .arg(name)
                                    .arg(formatFraction(std::fabs(value), false, true))
                                    .arg(qtTrId("charinfo_damage_reduction_header_tooltip"));
        QString style = QStringLiteral("Dialog");
        if (value > 0.0)
            style = QStringLiteral("ModifierPositive");
        if (value < 0.0)
            style = QStringLiteral("ModifierNegative");
        m_damageReductions.append(new TQmlStatWithSources(name, tooltip, formatFraction(value, true, true), true, std::move(sources), style,
                                                          { damageTypeIcon(resistance.damageType), QString() }, this));
    }
}

namespace {

using game::IMiscStatsProvider;

/// The skills widget's PlayerSkills stats: totals only.
class CombatStatsMiscProvider final : public IMiscStatsProvider
{
public:
    explicit CombatStatsMiscProvider(const CombatStats& stats)
        : m_stats(stats)
    {}

    int currentBlessings() const override { return 0; }
    int maxBlessings() const override { return 0; }
    std::vector<TimedEffect> activeConcoctions() const override { return {}; }
    std::vector<TimedEffect> consumableCooldowns() const override { return {}; }
    double momentum() const override { return m_stats.momentum; }
    std::optional<double> momentum(ECombatStatSource) const override { return std::nullopt; }
    double transcendence() const override { return m_stats.transcendence; }
    std::optional<double> transcendence(ECombatStatSource) const override { return std::nullopt; }
    double amplification() const override { return m_stats.amplification; }
    std::optional<double> amplification(ECombatStatSource) const override { return std::nullopt; }
    std::vector<SpellAugment> proficiencySpellAugments() const override { return {}; }
    std::vector<SpellAugment> skillWheelSpellAugments() const override { return {}; }
    std::vector<SpellAugment> equipmentSpellAugments() const override { return {}; }

private:
    CombatStats m_stats;
};

/// CIP: the concoctions and foods.
QList<QObject*> timedEffectData(const std::vector<IMiscStatsProvider::TimedEffect>& effects, QObject* parent)
{
    QList<QObject*> out;
    for (const IMiscStatsProvider::TimedEffect& effect : effects)
        out.append(new TConcoctionQmlData(effect.appearanceTypeId, effect.secondsRemaining, parent));
    return out;
}

/// CIP: one stat per spell, captioned with the spell's name, one row per
/// augment with its type name and value.
QList<QObject*> spellAugmentStats(const std::vector<IMiscStatsProvider::SpellAugment>& augments, QObject* parent)
{
    std::map<int, std::vector<IMiscStatsProvider::SpellAugment>> bySpell;
    for (const IMiscStatsProvider::SpellAugment& augment : augments)
        bySpell[augment.spellId].push_back(augment);
    QList<QObject*> out;
    for (const auto& [spellId, list] : bySpell) {
        QString caption;
        std::vector<StatSource> sources;
        for (const IMiscStatsProvider::SpellAugment& augment : list) {
            proficiency::TProficiencyPerk perk;
            perk.augmentType = augment.type;
            perk.spellId = augment.spellId;
            perk.value = augment.value;
            const proficiency::TAugmentTexts texts = proficiency::augmentTexts(perk);
            caption = texts.spellName;
            sources.push_back({ texts.typeName, texts.value, QString() });
        }
        out.append(makeSourcedStat(caption, QString(), QString(), true, std::move(sources), parent));
    }
    return out;
}

}

TConcoctionQmlData::TConcoctionQmlData(const int appearanceTypeId, const qlonglong secondsRemaining, QObject* parent)
    : cip::tibia::cyclopedia::TConcoctionQmlData(parent)
{
    m_appearanceTypeID = appearanceTypeId;
    m_secondsRemaining = secondsRemaining;
    if (const TAppearanceTypePtr& type = tibia::game::thingType(appearanceTypeId, ThingCategoryItem))
        m_name = QString::fromStdString(type->getName());
}

TMiscStatsQmlData::TMiscStatsQmlData(const CombatStats& stats, QObject* parent)
    : TMiscStatsQmlData(std::make_shared<CombatStatsMiscProvider>(stats), parent)
{}

TMiscStatsQmlData::TMiscStatsQmlData(std::shared_ptr<game::IMiscStatsProvider> provider, QObject* parent)
    : cip::tibia::cyclopedia::TMiscStatsQmlData(parent)
    , m_provider(std::move(provider))
{
    const IMiscStatsProvider& p = *m_provider;
    const std::shared_ptr<IMiscStatsProvider> shared = m_provider;
    m_momentum = makePercentStat("momentum_caption", qtTrId("momentum_tooltip").arg(formatFraction(p.momentum(), false, true)),
                                 p.momentum(), [shared](ECombatStatSource s) { return shared->momentum(s); }, this);
    m_transcendence = makePercentStat(
        "transcendence_caption", qtTrId("transcendence_tooltip").arg(formatFraction(p.transcendence(), false, true)),
        p.transcendence(), [shared](ECombatStatSource s) { return shared->transcendence(s); }, this);
    m_amplification = makePercentStat(
        "amplification_caption", qtTrId("amplification_tooltip").arg(formatFraction(p.amplification(), false, true)),
        p.amplification(), [shared](ECombatStatSource s) { return shared->amplification(s); }, this);
    m_activeConcoctions = timedEffectData(p.activeConcoctions(), this);
    m_consumableCooldowns = timedEffectData(p.consumableCooldowns(), this);
    m_spellAugments = spellAugmentStats(p.proficiencySpellAugments(), this);
    m_skillwheelSpellAugments = spellAugmentStats(p.skillWheelSpellAugments(), this);
    m_equipmentSpellAugments = spellAugmentStats(p.equipmentSpellAugments(), this);
}

int TMiscStatsQmlData::currentBlessings() const { return m_provider->currentBlessings(); }

int TMiscStatsQmlData::maxBlessings() const { return m_provider->maxBlessings(); }

QString TMiscStatsQmlData::blessingButtonColor() const
{
    if (m_provider->currentBlessings() == m_provider->maxBlessings())
        return QStringLiteral("green");
    if (m_provider->currentBlessings() >= 1)
        return QStringLiteral("gold");
    return QStringLiteral("grey");
}

}
