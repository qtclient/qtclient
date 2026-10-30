#pragma once

#include <cip_gen.h>

#include <map>
#include <vector>

namespace tibia::cyclopedia {

/// One source row of a stat (CIP: the 0x48-byte {name, value, tooltip} entries).
struct StatSource
{
    QString name;
    QString value;
    QString tooltip;
};

/// A stat's sources, roles name / value / tooltip (CIP: TStatSourcesListModel,
/// vtable 0x102626b28).
class TStatSourcesListModel final : public cip::tibia::cyclopedia::TStatSourcesListModel
{
    Q_OBJECT
public:
    explicit TStatSourcesListModel(std::vector<StatSource> sources, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<StatSource> m_sources;
};

/// One stat block of CharacterCombatStatBlock.qml (CIP: TQmlStatWithSources,
/// ctor 0x100a992b0; FUN_100a9e9b0 passes style "Dialog" and no icon).
class TQmlStatWithSources final : public cip::tibia::cyclopedia::TQmlStatWithSources
{
    Q_OBJECT
public:
    struct Icon
    {
        QUrl source;
        QString tooltip;
    };

    TQmlStatWithSources(const QString& caption, const QString& tooltip, const QString& total, bool visible,
                        std::vector<StatSource> sources = {}, const QString& styleType = QStringLiteral("Dialog"),
                        const Icon& icon = {}, QObject* parent = nullptr);
};

/// CIP FUN_10095c380: `value` in English notation with at most `maxDecimals`
/// decimals; trailing zeros are dropped, or zeros padded up to `minDecimals`.
QString formatDecimal(double value, int minDecimals, int maxDecimals);
/// CIP FUN_100a9ecc0: a fraction as a percentage number (x100, up to 2
/// decimals), "+" before non-negative values when `showSign`, "%" after when
/// `showPercent`.
QString formatFraction(double value, bool showPercent, bool showSign);
/// CIP FUN_100aa04d0: an English-notation integer, "+" first when `showSign`.
QString formatInteger(qint64 value, bool showSign);
/// CIP FUN_100a45e80: the damage type's name (0 physical .. 11 agony; -1 none).
QString damageTypeName(int damageType);
/// CIP FUN_1009260f0: "/images/monster-icon-<name>-resist.png", the lower-case
/// name without spaces.
QUrl damageTypeIcon(int damageType);

/// The combat stats of the PlayerSkills message (CIP: game::TPlayerSkillStats,
/// filled from protobuf PlayerSkills; IOffenceStats / IDefenceStats /
/// IMiscStats getters 0x100ae0bb0..0x100ae18f0). Fractions: 0.05 is 5%.
/// Damage types use CIP's numbering (the protocol's map 1:1, FUN_100f51300).
/// PlayerSkills carries no stat sources, so every source list stays empty.
struct CombatStats
{
    int flatDamageHealing = 0;
    int attackValue = 0;
    int attackDamageType = 0;
    double convertedDamage = 0;
    int convertedDamageType = 0;
    double lifeLeech = 0;
    double manaLeech = 0;
    double critChance = 0;
    double critExtraDamage = 0;
    double onslaught = 0;

    int defence = 0;
    int armor = 0;
    int mantra = 0;
    double mitigation = 0;
    double dodge = 0;
    int damageReflection = 0;
    /// Damage type -> reduction (negative: the damage increases).
    std::map<int, double> damageReductions;

    double momentum = 0;
    double transcendence = 0;
    double amplification = 0;
};

/// The offence block (CIP: TOffenceStatsQmlData, ctor 0x100a99640). Filled:
/// the PlayerSkills stats (crit chance / extra damage, flat damage and
/// healing, attack value, special attack conversion, life / mana leech,
/// onslaught); the Cyclopedia-only blocks stay null.
class TOffenceStatsQmlData final : public cip::tibia::cyclopedia::TOffenceStatsQmlData
{
    Q_OBJECT
public:
    explicit TOffenceStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);
};

/// The defence block (CIP: TDefenceStatsQmlData, ctor 0x100aa34e0).
class TDefenceStatsQmlData final : public cip::tibia::cyclopedia::TDefenceStatsQmlData
{
    Q_OBJECT
public:
    explicit TDefenceStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);
};

/// The misc block (CIP: TMiscStatsQmlData, ctor 0x100aa6750). Filled: momentum,
/// transcendence, amplification; the Cyclopedia-only lists and blessings stay
/// empty.
class TMiscStatsQmlData final : public cip::tibia::cyclopedia::TMiscStatsQmlData
{
    Q_OBJECT
public:
    explicit TMiscStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);
};

}
