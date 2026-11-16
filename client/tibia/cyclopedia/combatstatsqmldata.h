#pragma once

#include <cip_gen.h>

#include "game/miscstatsprovider.h"
#include "game/offencestatsprovider.h"

#include <map>
#include <memory>
#include <vector>

namespace tibia {
namespace cyclopedia {

/// One source row of a stat (CIP: the 0x48-byte {name, value, tooltip} entries).
struct StatSource
{
    QString name;
    QString value;
    QString tooltip;
};

/// A stat's sources, roles name / value / tooltip (CIP: TStatSourcesListModel,
/// vtable).
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
/// ctor ; passes style "Dialog" and no icon).
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

/// CIP : `value` in English notation with at most `maxDecimals`
/// decimals; trailing zeros are dropped, or zeros padded up to `minDecimals`.
QString formatDecimal(double value, int minDecimals, int maxDecimals);
/// CIP : a fraction as a percentage number (x100, up to 2
/// decimals), "+" before non-negative values when `showSign`, "%" after when
/// `showPercent`.
QString formatFraction(double value, bool showPercent, bool showSign);
/// CIP : an English-notation integer, "+" first when `showSign`.
QString formatInteger(qint64 value, bool showSign);
/// CIP : the damage type's name (0 physical .. 11 agony; -1 none).
QString damageTypeName(int damageType);
/// CIP : "/images/monster-icon-<name>-resist.png", the lower-case
/// name without spaces.
QUrl damageTypeIcon(int damageType);

/// The combat stats of the PlayerSkills message (CIP: game::TPlayerSkillStats,
/// filled from protobuf PlayerSkills; IOffenceStats / IDefenceStats /
/// IMiscStats getters ..). Fractions: 0.05 is 5%.
/// Damage types use CIP's numbering (the protocol's map 1:1,).
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

/// The offence block of the character info's combat stats (CIP:
/// TOffenceStatsQmlData). Every stat is built once from the provider.
class TOffenceStatsQmlData final : public cip::tibia::cyclopedia::TOffenceStatsQmlData
{
    Q_OBJECT
public:
    explicit TOffenceStatsQmlData(std::shared_ptr<game::IOffenceStatsProvider> provider, QObject* parent = nullptr);
    /// The skills widget's block: the PlayerSkills stats as a provider.
    explicit TOffenceStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);

private:
    /// CIP: the element spells, offensive runes and auto attack crit chances.
    QObject* makeSpecialCritChances();
    /// CIP: the same for the crit extra damage.
    QObject* makeSpecialCritDamages();
    /// CIP: one row per damage type.
    QObject* makeElementalPierce();
    /// CIP: one row per skill; the total is the rounded sum.
    QObject* makeAutoAttackBonusFromSkills();
    QObject* makeSpellBonusDamageFromSkills();
    QObject* makeSpellBonusHealingFromSkills();

    std::shared_ptr<game::IOffenceStatsProvider> m_provider;
};

/// The defence block (CIP: TDefenceStatsQmlData, ctor).
class TDefenceStatsQmlData final : public cip::tibia::cyclopedia::TDefenceStatsQmlData
{
    Q_OBJECT
public:
    explicit TDefenceStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);
};

/// An active concoction or food of the misc block (CIP: TConcoctionQmlData).
class TConcoctionQmlData final : public cip::tibia::cyclopedia::TConcoctionQmlData
{
    Q_OBJECT
public:
    TConcoctionQmlData(int appearanceTypeId, qlonglong secondsRemaining, const QString& name, QObject* parent = nullptr);
};

/// The misc block of the character info's combat stats (CIP:
/// TMiscStatsQmlData). The blessings are read from the provider.
class TMiscStatsQmlData final : public cip::tibia::cyclopedia::TMiscStatsQmlData
{
    Q_OBJECT
public:
    explicit TMiscStatsQmlData(std::shared_ptr<game::IMiscStatsProvider> provider, QObject* parent = nullptr);
    /// The skills widget's block: the PlayerSkills stats as a provider.
    explicit TMiscStatsQmlData(const CombatStats& stats, QObject* parent = nullptr);

    int currentBlessings() const override;
    int maxBlessings() const override;
    /// CIP: "green" with every blessing, "gold" with some, else "grey".
    QString blessingButtonColor() const override;

private:
    std::shared_ptr<game::IMiscStatsProvider> m_provider;
};

}
}
