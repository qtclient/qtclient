#include "skillwheeldata.h"

#include "spells/spellinfostorage.h"

#include <QByteArray>
#include <QLocale>
#include <QStringList>

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace tibia::skillwheel {

namespace {

struct TBasicEffect
{
    int stat;
    double values[VocationCount][4];
};

struct TBasicMod
{
    int count;
    TBasicEffect effects[2];
};

struct TSupremeAugmentation
{
    int spellId;
    int augType;
    double values[4];
    bool valid;
};

#include "skillwheelmodtables.inc"

/// One augmentation of a conviction perk's level (CIP: an element of the
/// 0x1077b3778 level lists, of which 0x100fabf50 renders the map's first spell).
struct TAugmentation
{
    int perk;
    int level;
    int augType;
    int spellId;
    double value;
};

#include "skillwheelaugtable.inc"

constexpr int BasicModCount = static_cast<int>(std::size(BasicMods));
constexpr int SupremeModCount = static_cast<int>(std::size(SupremeCategories));

const QChar MultiStringSeparator(0x9c);

constexpr std::array<int, SlotCount> MaxPoints = {
    200, 150, 100, 100, 150, 200,
    150, 100, 75, 75, 100, 150,
    100, 75, 50, 50, 75, 100,
    100, 75, 50, 50, 75, 100,
    150, 100, 75, 75, 100, 150,
    200, 150, 100, 100, 150, 200,
};

constexpr std::array<int, SlotCount> SmallPerks = {
    2, 4, 0, 1, 0, 2,
    4, 0, 1, 3, 1, 0,
    0, 1, 3, 4, 3, 1,
    4, 0, 1, 0, 4, 3,
    3, 4, 0, 4, 3, 1,
    2, 3, 4, 3, 1, 2,
};

/// Life / mana / capacity per point by vocation index (CIP 0x10232e238 and siblings).
constexpr int LifePerPoint[VocationCount] = { 3, 2, 1, 1, 2 };
constexpr int ManaPerPoint[VocationCount] = { 1, 3, 6, 6, 2 };
constexpr int CapacityPerPoint[VocationCount] = { 5, 4, 2, 2, 5 };
/// CIP DAT_1017dc404.
constexpr float MitigationPerPoint = 0.075f;

constexpr std::array<std::array<int, VocationCount>, SlotCount> MediumPerks = { {
    { 8, 17, 35, 28, 42 }, { 5, 5, 5, 5, 5 }, { 37, 37, 37, 37, 37 }, { 7, 15, 34, 34, 43 }, { 38, 38, 38, 38, 38 }, { 12, 22, 27, 29, 47 },
    { 37, 37, 37, 37, 37 }, { 14, 21, 26, 32, 45 }, { 6, 6, 6, 6, 6 }, { 38, 38, 38, 38, 38 }, { 13, 20, 36, 33, 46 }, { 5, 5, 5, 5, 5 },
    { 11, 19, 25, 30, 48 }, { 7, 15, 34, 34, 43 }, { 37, 37, 37, 37, 37 }, { 10, 18, 24, 31, 44 }, { 6, 6, 6, 6, 6 }, { 38, 38, 38, 38, 38 },
    { 39, 39, 39, 39, 39 }, { 5, 5, 5, 5, 5 }, { 12, 22, 27, 29, 47 }, { 40, 40, 40, 40, 40 }, { 7, 15, 34, 34, 43 }, { 14, 21, 26, 32, 45 },
    { 6, 6, 6, 6, 6 }, { 13, 20, 36, 33, 46 }, { 39, 39, 39, 39, 39 }, { 5, 5, 5, 5, 5 }, { 11, 19, 25, 30, 48 }, { 40, 40, 40, 40, 40 },
    { 10, 18, 24, 31, 44 }, { 39, 39, 39, 39, 39 }, { 7, 15, 34, 34, 43 }, { 40, 40, 40, 40, 40 }, { 6, 6, 6, 6, 6 }, { 9, 16, 23, 35, 41 },
} };

constexpr std::array<std::array<int, 9>, DomainCount> DomainSlots = { {
    { 14, 13, 12, 8, 7, 6, 2, 1, 0 },
    { 15, 16, 17, 9, 10, 11, 3, 4, 5 },
    { 20, 19, 18, 26, 25, 24, 32, 31, 30 },
    { 21, 22, 23, 27, 28, 29, 33, 34, 35 },
} };

constexpr std::array<std::array<int, VocationCount>, DomainCount> LargePerks = { {
    { 0, 0, 0, 0, 0 },
    { 1, 4, 7, 11, 13 },
    { 2, 5, 8, 10, 14 },
    { 3, 6, 9, 12, 15 },
} };

constexpr std::array<int, 4> CentreSlots = { 14, 15, 20, 21 };

/// Neighbours across the domain borders that are not orthogonal grid neighbours.
constexpr std::pair<int, int> ExtraLinks[] = {
    { 1, 6 }, { 2, 7 }, { 3, 10 }, { 4, 11 }, { 6, 1 }, { 7, 2 }, { 7, 12 }, { 8, 13 }, { 9, 16 }, { 10, 3 }, { 10, 17 },
    { 11, 4 }, { 12, 7 }, { 13, 8 }, { 16, 9 }, { 17, 10 }, { 18, 25 }, { 19, 26 }, { 22, 27 }, { 23, 28 }, { 24, 31 },
    { 25, 18 }, { 25, 32 }, { 26, 19 }, { 27, 22 }, { 28, 23 }, { 28, 33 }, { 29, 34 }, { 31, 24 }, { 32, 25 }, { 33, 28 }, { 34, 29 },
};

constexpr int Thresholds[] = { 0, 250, 500, 1000 };
constexpr int RevelationMasteryBonus[] = { 150, 165, 180, 225 };

/// Supreme revelation mastery mods and the large perk each raises.
constexpr std::pair<int, int> RevelationMasteryPerks[] = {
    { 5, 0 }, { 0x15, 3 }, { 0x16, 1 }, { 0x17, 2 }, { 0x27, 6 }, { 0x28, 4 }, { 0x29, 5 }, { 0x38, 9 },
    { 0x39, 7 }, { 0x3a, 8 }, { 0x49, 12 }, { 0x4a, 11 }, { 0x4b, 10 }, { 0x5b, 15 }, { 0x5c, 13 }, { 0x5d, 14 },
};

/// Cooldown augmentations that also grant momentum.
constexpr int MomentumMods[] = { 6, 7, 0x18, 0x19, 0x24, 0x25, 0x2a, 0x2b, 0x3b, 0x3c, 0x4c, 0x58, 0x59 };
constexpr double MomentumByGrade[] = { 0, 0.33, 0.66, 1.0 };

constexpr double DodgeByGrade[] = { 0.28, 0.31, 0.34, 0.42 };
constexpr double CriticalByGrade[] = { 2, 2.2, 2.4, 3 };
constexpr double LifeLeechByGrade[] = { 2, 2.2, 2.4, 3 };
constexpr double ManaLeechByGrade[] = { 0.8, 0.88, 0.96, 1.2 };

constexpr const char* DamageTypeIds[] = {
    "damagetype_physical", "damagetype_holy", "damagetype_death", "damagetype_fire", "damagetype_earth",
    "damagetype_ice", "damagetype_energy", "damagetype_manadrain", "damagetype_lifeleech",
};

constexpr const char* LargePerkNames[] = {
    "perk_name_GiftOfLife", "perk_name_ExecutionersThrow", "perk_name_CombatMastery", "perk_name_AvatarOfSteel",
    "perk_name_DivineGrenade", "perk_name_DivineEmpowerment", "perk_name_AvatarOfLight", "perk_name_BeamMastery",
    "perk_name_LordOfDestruction", "perk_name_AvatarOfStorm", "perk_name_TwinBursts", "perk_name_BlessingOfTheGrove",
    "perk_name_AvatarOfNature", "perk_name_SpiritualOutburst", "perk_name_Ascetic", "perk_name_AvatarOfBalance",
};

/// Conviction perks 5..48 (CIP 0x100faaa60): the name id. Spell augmentations
/// and the Aug* perks get the augmentation prefix, vessels 37..40 the resonance one.
const char* mediumPerkId(const int perk)
{
    switch (perk) {
        case 5: return "perk_name_ManaLeech";
        case 6: return "perk_name_LifeLeech";
        case 7: return "perk_name_SkillBonusAxeClubSword";
        case 8: return "perk_name_BattleInstinct";
        case 9: return "perk_name_BattleHealing";
        case 10: return "spell_name_exori_gran";
        case 11: return "spell_name_exura_gran_ico";
        case 12: return "spell_name_exori_min";
        case 13: return "spell_name_exori_mas";
        case 14: return "spell_name_exori_scu";
        case 15: return "perk_name_SkillBonusDistance";
        case 16: return "perk_name_BallisticMastery";
        case 17: return "perk_name_PositionalTactics";
        case 18: return "spell_name_exevo_mas_san";
        case 19: return "spell_name_exori_dir_san";
        case 20: return "spell_name_exana_amp_res";
        case 21: return "spell_name_exori_gran_con";
        case 22: return "spell_name_exori_dir_moe";
        case 23: return "perk_name_FocusMastery";
        case 24: return "spell_name_exevo_gran_flam_hur";
        case 25: return "spell_name_exevo_vis_hur";
        case 26: return "perk_name_AugSpecialSpells";
        case 27: return "perk_name_AugFocusSpells";
        case 28: return "perk_name_HealingLink";
        case 29: return "perk_name_AugForkedSpells";
        case 30: return "spell_name_exevo_tera_hur";
        case 31: return "spell_name_exevo_gran_frigo_hur";
        case 32: return "spell_name_exura_gran_mas_res";
        case 33: return "spell_name_exura_sio";
        case 34: return "perk_name_SkillBonusMagic";
        case 35: return "perk_name_RunicMastery";
        case 36: return "spell_name_exevo_mort_ora";
        case 37: return "skill_wheel_domain_name_topleft";
        case 38: return "skill_wheel_domain_name_topright";
        case 39: return "skill_wheel_domain_name_bottomleft";
        case 40: return "skill_wheel_domain_name_bottomright";
        case 41: return "perk_name_Sanctuary";
        case 42: return "perk_name_GuidingPresence";
        case 43: return "perk_name_SkillBonusFistFighting";
        case 44: return "spell_name_exori_mas_amp_pug";
        case 45: return "spell_name_exura_mas_nia";
        case 46: return "spell_name_exori_amp_pug";
        case 47: return "spell_name_exori_med_pug";
        case 48: return "spell_name_exori_mas_pug";
        default: return "dummy_unknown";
    }
}

QString multi(const QString& longText, const QString& shortText) { return longText + MultiStringSeparator + shortText; }

/// CIP 0x10095c380: up to `decimals` decimals, trailing zeros dropped.
QString trimmedNumber(const double value, const int decimals)
{
    QString text = QLocale(QLocale::English).toString(value, 'f', decimals);
    if (text.contains(u'.')) {
        while (text.endsWith(u'0'))
            text.chop(1);
        if (text.endsWith(u'.'))
            text.chop(1);
    }
    return text;
}

QString sign(const double value) { return value > 0.0 ? QStringLiteral("+") : QString(); }

/// CIP 0x100fa2360: a mod value (scaled by 100) in the given format.
QString formatModValue(const qint64 value, const int format)
{
    switch (format) {
        case 1: return sign(static_cast<double>(value)) + englishNumber(value);
        case 4: {
            const double v = static_cast<double>(value) * 0.0001 * 100.0;
            return QStringLiteral("%1%").arg(sign(v) + trimmedNumber(v, 2));
        }
        case 6: return QStringLiteral("%1%").arg(QLocale(QLocale::English).toString(static_cast<double>(value) * 0.0001 * 100.0, 'f', 2));
        default: return qtTrId("dummy_unknown");
    }
}

/// CIP 0x10102bcc0: a spell group's name. CIP's ids (spells.json parse
/// 0x100fe6a30) are 9 great beams and 10 bursts of nature; TSpellInfo keeps
/// Crystal's protocol ids (SpellGroup_t), which swap the two.
QString spellGroupName(const int protocolGroup)
{
    const int group = protocolGroup == 9 ? 10 : protocolGroup == 10 ? 9 : protocolGroup;
    switch (group) {
        case 1: return qtTrId("spell_attack");
        case 2: return qtTrId("spell_healing");
        case 3: return qtTrId("spell_support");
        case 4: return qtTrId("spellcooldown_spell_special");
        case 5: return qtTrId("spellcooldown_spell_conjure");
        case 6: return qtTrId("spellcooldown_spell_crippling");
        case 7: return qtTrId("spellcooldown_spell_focus");
        case 8: return qtTrId("spellcooldown_spell_ultimate_strikes");
        case 9: return qtTrId("spellcooldown_spell_great_beams");
        case 10: return qtTrId("spellcooldown_spell_bursts_of_nature");
        case 11: return qtTrId("spellcooldown_spell_stance");
        default: return QString();
    }
}

/// CIP 0x100960d30: `value` * 100 in English with `decimals` decimals ('f'),
/// "+" for positive values when `plus`.
QString percentNumber(const double value, const int decimals, const bool plus)
{
    const double v = value * 100.0;
    const int precision = v != 0.0 ? decimals : std::min(decimals, 0);
    return QString::fromLatin1(plus && value > 0.0 ? "+" : "") + QLocale(QLocale::English).toString(v, 'f', precision);
}

/// CIP 0x100960f10 (via 0x10095c380): `value` * 100 with up to `decimals` decimals.
QString trimmedPercentNumber(const double value, const int decimals) { return trimmedNumber(value * 100.0, decimals); }

/// CIP 0x10095e540: minutes as "1h 30min".
QString compactMinutes(const int minutes)
{
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    if (minutes % 60 < 1)
        return qtTrId("time_hour_short").arg(minutes / 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

/// CIP 0x100fa8da0: the three stage values of a large perk, the reached stage
/// in white and the others in #3F3F3F.
QString stageValues(const QString& stage1, const QString& stage2, const QString& stage3, const int stage)
{
    const auto color = [stage](const int s) { return QLatin1String(stage == s ? "#ffffff" : "#3f3f3f"); };
    return qtTrId("skill_wheel_dialog_large_perk_effect_values_format")
        .arg(stage1, color(1), stage2, color(2), stage3, color(3));
}

QString stageValues(const int stage1, const int stage2, const int stage3, const int stage)
{
    return stageValues(QString::number(stage1), QString::number(stage2), QString::number(stage3), stage);
}

QString stagePercents(const int stage1, const int stage2, const int stage3, const int stage)
{
    const auto percent = [](const int v) { return QString::number(v) + u'%'; };
    return stageValues(percent(stage1), percent(stage2), percent(stage3), stage);
}

/// CIP QString::number(v, <invalid format>, 0): decimal without decimals.
QString wholeNumber(const double value) { return QString::number(value, 'f', 0); }

bool connected(const int a, const int b)
{
    const int ra = a / 6, ca = a % 6, rb = b / 6, cb = b % 6;
    if ((ra == rb && std::abs(ca - cb) == 1) || (ca == cb && std::abs(ra - rb) == 1))
        return true;
    return std::find(std::begin(ExtraLinks), std::end(ExtraLinks), std::pair{ a, b }) != std::end(ExtraLinks);
}

}

int maxPoints(const int slot) { return slot >= 0 && slot < SlotCount ? MaxPoints[slot] : 0; }

ESmallPerk smallPerk(const int slot) { return static_cast<ESmallPerk>(slot >= 0 && slot < SlotCount ? SmallPerks[slot] : 0); }

int mediumPerk(const int slot, const int vocationIndex)
{
    if (slot < 0 || slot >= SlotCount || vocationIndex < 0 || vocationIndex >= VocationCount)
        return 0;
    return MediumPerks[slot][vocationIndex];
}

bool isVesselPerk(const int perk) { return perk >= 37 && perk <= 40; }

bool isAugmentedPerk(const int perk) { return perk >= 0 && perk < 0x31 && ((0x1f013ef7c7c00ULL >> perk) & 1) != 0; }

const std::array<int, 9>& domainSlots(const int domain) { return DomainSlots[std::clamp(domain, 0, DomainCount - 1)]; }

int domainOfSlot(const int slot)
{
    for (int d = 0; d < DomainCount; ++d)
        if (std::ranges::find(DomainSlots[d], slot) != DomainSlots[d].end())
            return d;
    return 0;
}

int largePerk(const int domain, const int vocationIndex)
{
    if (domain < 0 || domain >= DomainCount || vocationIndex < 0 || vocationIndex >= VocationCount)
        return 0;
    return LargePerks[domain][vocationIndex];
}

bool isValid(const std::array<int, SlotCount>& points)
{
    std::array<bool, SlotCount> reached{};
    std::vector<int> open;
    for (const int c : CentreSlots)
        if (points[c] > 0) {
            reached[c] = true;
            open.push_back(c);
        }
    while (!open.empty()) {
        const int slot = open.back();
        open.pop_back();
        if (points[slot] < MaxPoints[slot])
            continue;
        for (int next = 0; next < SlotCount; ++next)
            if (!reached[next] && points[next] > 0 && connected(slot, next)) {
                reached[next] = true;
                open.push_back(next);
            }
    }
    for (int slot = 0; slot < SlotCount; ++slot)
        if (points[slot] > MaxPoints[slot] || (points[slot] > 0 && !reached[slot]))
            return false;
    return true;
}

bool isUnlocked(const std::array<int, SlotCount>& points, const int slot)
{
    if (points[slot] > 0)
        return true;
    auto filled = points;
    filled[slot] = MaxPoints[slot];
    return isValid(filled);
}

bool canDecrease(const std::array<int, SlotCount>& points, const int slot)
{
    if (points[slot] < MaxPoints[slot])
        return points[slot] > 0;
    auto cleared = points;
    cleared[slot] = 0;
    return isValid(cleared);
}

int resonance(const std::array<int, SlotCount>& points, const int domain)
{
    int full = 0;
    for (const int slot : domainSlots(domain))
        if (isVesselPerk(MediumPerks[slot][0]) && points[slot] >= MaxPoints[slot])
            ++full;
    return full;
}

int revelationPoints(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& supremeGrades, const int domain, const int vocationIndex)
{
    int total = 0;
    for (const int slot : domainSlots(domain))
        total += wheel.points[slot];
    const int perk = largePerk(domain, vocationIndex);
    for (int d = 0; d < DomainCount; ++d) {
        if (wheel.gems[d] == NoGem || resonance(wheel.points, d) < 3)
            continue;
        const auto it = std::ranges::find_if(gems, [&](const TGem& g) { return g.id == wheel.gems[d]; });
        if (it == gems.end() || it->quality < 2 || revelationMasteryPerk(it->supreme) != perk)
            continue;
        const auto grade = supremeGrades.find(it->supreme);
        total += revelationMasteryBonus(grade != supremeGrades.end() ? grade->second : 0);
    }
    return std::min(total, 1000);
}

int revelationStage(const int points)
{
    int stage = 0;
    while (stage < 3 && points >= Thresholds[stage + 1])
        ++stage;
    return stage;
}

int revelationThreshold(const int stage) { return Thresholds[std::clamp(stage, 0, 3)]; }

int modCount(const TGem& gem) { return std::clamp(gem.quality, 0, 2) + 1; }

bool isSupremeModIndex(const int modIndex) { return modIndex == 2; }

int modId(const TGem& gem, const int modIndex)
{
    switch (modIndex) {
        case 0: return gem.basic1;
        case 1: return gem.basic2;
        default: return gem.supreme;
    }
}

int gemGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, const int modIndex)
{
    const TGrades& grades = isSupremeModIndex(modIndex) ? supremeGrades : basicGrades;
    const auto it = grades.find(modId(gem, modIndex));
    return it != grades.end() ? it->second : 0;
}

int effectiveGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, const int modIndex)
{
    int grade = gemGrade(gem, basicGrades, supremeGrades, modIndex);
    for (int i = 0; i < modIndex; ++i)
        grade = std::min(grade, gemGrade(gem, basicGrades, supremeGrades, i));
    return grade;
}

QString englishNumber(const qint64 value) { return QLocale(QLocale::English).toString(value); }

QString smallPerkName(const ESmallPerk perk)
{
    switch (perk) {
        case ESmallPerk::Life: return qtTrId("hit_points");
        case ESmallPerk::Mana: return qtTrId("mana");
        case ESmallPerk::Capacity: return qtTrId("capacity");
        case ESmallPerk::Mitigation: return QStringList{ qtTrId("mitigation_multiplier"), qtTrId("mitigation_multiplier_short") }.join(MultiStringSeparator);
        default: return qtTrId("dummy_unknown");
    }
}

int smallPerkValue(const ESmallPerk perk, const int points, const int vocationIndex)
{
    if (vocationIndex < 0 || vocationIndex >= VocationCount)
        return 0;
    switch (perk) {
        case ESmallPerk::Life: return LifePerPoint[vocationIndex] * points;
        case ESmallPerk::Mana: return ManaPerPoint[vocationIndex] * points;
        case ESmallPerk::Capacity: return CapacityPerPoint[vocationIndex] * points;
        default: return 0;
    }
}

double mitigationValue(const int points) { return static_cast<float>(points) * MitigationPerPoint; }

QString smallPerkValueText(const ESmallPerk perk, const int points, const int vocationIndex)
{
    switch (perk) {
        case ESmallPerk::Life:
        case ESmallPerk::Mana:
        case ESmallPerk::Capacity:
            return QStringLiteral("+%1 %2").arg(englishNumber(smallPerkValue(perk, points, vocationIndex)), smallPerkName(perk));
        case ESmallPerk::LifeAndMana:
            return QStringLiteral("%1<br>%2").arg(smallPerkValueText(ESmallPerk::Life, points, vocationIndex), smallPerkValueText(ESmallPerk::Mana, points, vocationIndex));
        case ESmallPerk::Mitigation: {
            const double value = mitigationValue(points);
            return QStringLiteral("%1% %2").arg(QLocale(QLocale::English).toString(value, 'f', value != 0.0 ? 2 : 0), multiPart(smallPerkName(perk), 0));
        }
    }
    return qtTrId("dummy_unknown");
}

QString smallPerkInfo(const ESmallPerk perk, const int vocationIndex)
{
    QStringList lines;
    if (perk == ESmallPerk::Mitigation)
        lines << qtTrId("skill_wheel_dialog_mitigation_perk_info") << QString();
    lines << qtTrId("skill_wheel_dialog_small_perk_per_point") << smallPerkValueText(perk, 1, vocationIndex);
    return lines.join(QStringLiteral("<br>"));
}

QString multiPart(const QString& text, const int index)
{
    const QStringList parts = text.split(MultiStringSeparator);
    return parts.value(index, parts.first());
}

QString mediumPerkName(const int perk)
{
    const QString name = qtTrId(mediumPerkId(perk));
    if (isAugmentedPerk(perk))
        return multi(qtTrId("perk_name_prefix_augmentation").arg(name), qtTrId("perk_name_prefix_augmentation_short").arg(name));
    if (isVesselPerk(perk))
        return multi(qtTrId("perk_name_VesselResonance").arg(name), qtTrId("perk_name_VesselResonance_short").arg(name));
    return name;
}

QString largePerkName(const int perk)
{
    if (perk < 0 || perk >= static_cast<int>(std::size(LargePerkNames)))
        return qtTrId("dummy_unknown");
    return qtTrId(LargePerkNames[perk]);
}

QString largePerkShortInformation(const int perk)
{
    switch (perk) {
        case 0: return qtTrId("perk_description_short_GiftOfLife");
        case 1: return qtTrId("perk_description_short_ExecutionersThrow");
        case 2: return qtTrId("perk_description_short_CombatMastery");
        case 3: case 6: case 9: case 12: case 15: return qtTrId("perk_description_short_AvatarOfVocation");
        case 4: return qtTrId("perk_description_short_DivineGrenade");
        case 5: return qtTrId("perk_description_short_DivineEmpowerment");
        case 7: return qtTrId("perk_description_short_BeamMastery");
        case 8: return qtTrId("perk_description_short_LordOfDestruction");
        case 10: return qtTrId("perk_description_short_TwinBursts");
        case 11: return qtTrId("perk_description_short_BlessingOfTheGrove");
        case 13: return qtTrId("perk_description_short_SpiritualOutburst");
        case 14: return qtTrId("perk_description_short_Ascetic");
        default: return qtTrId("dummy_unknown");
    }
}

int mediumPerkCount(const std::array<int, SlotCount>& points, const int perk, const int vocationIndex)
{
    int count = 0;
    for (int slot = 0; slot < SlotCount; ++slot)
        if (mediumPerk(slot, vocationIndex) == perk && points[slot] >= MaxPoints[slot])
            ++count;
    return count;
}

QString mediumPerkValueText(const std::array<int, SlotCount>& points, const int perk, const int vocationIndex)
{
    const int count = mediumPerkCount(points, perk, vocationIndex);
    switch (perk) {
        case 0: case 1: case 2: case 3: return formatValue(count * 2, 2);
        case 4: return qtTrId("dummy_unknown");
        case 5: return formatValue(count * 25, 3);
        case 6: return formatValue(count * 75, 3);
        case 7: case 15: case 34: case 43: return formatValue(count, 1);
        default: return formatValue(count, isAugmentedPerk(perk) || isVesselPerk(perk) ? 7 : 0);
    }
}

QString augmentationDescription(const double value, const int spellId, const int augType, const double momentum, const bool hasMomentum,
                                 const bool shortForm)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
    if (!spell)
        return qtTrId("dummy_unknown");
    switch (augType) {
        case 1: return qtTrId("augmentation_description_mana_costs").arg(value);
        case 2: return qtTrId("augmentation_description_percentage_damage").arg(value * 100.0);
        case 3: return qtTrId("augmentation_description_percentage_healing").arg(value * 100.0);
        case 4: return qtTrId("augmentation_description_duration");
        case 5:
            if (value != 1.0)
                return qtTrId("augmentation_description_chain_length_plural").arg(value);
            return qtTrId("augmentation_description_chain_length_singular").arg(1.0);
        case 6:
            if (hasMomentum && momentum > 0.0)
                return qtTrId("augmentation_description_cooldown_with_momentum").arg(value).arg(momentum);
            return qtTrId("augmentation_description_cooldown").arg(value);
        case 7: return qtTrId("augmentation_description_secondary_group_cooldown").arg(spellGroupName(spell->groupSecondary)).arg(value);
        case 8: case 11: return qtTrId("augmentation_description_affected_area");
        case 9: return qtTrId("augmentation_description_sap_strength").arg(value);
        case 10: return qtTrId("augmentation_description_sap_range").arg(value);
        case 12: return qtTrId("augmentation_description_boosted_effect");
        case 13: {
            const QString skill = spellId == 0x139 ? qtTrId("distance_fighting").toLower() : QString();
            return qtTrId("augmentation_description_skill_increase").arg(skill).arg(value);
        }
        case 14: return qtTrId("augmentation_description_life_leech").arg(value);
        case 15: return qtTrId("augmentation_description_mana_leech").arg(value);
        case 16:
            return qtTrId(shortForm ? "augmentation_description_critical_damage_short" : "augmentation_description_critical_damage").arg(value * 100.0);
        case 17: return qtTrId("augmentation_description_critical_chance").arg(value * 100.0);
        case 18: return qtTrId("augmentation_description_mean_damage");
        case 19: return qtTrId("augmentation_description_shape");
        case 20: return qtTrId("augmentation_description_delay").arg(std::fabs(value));
        case 21: return qtTrId("augmentation_description_nextautoattackreduction").arg(value * -100.0);
        default: return qtTrId("dummy_unknown");
    }
}

QStringList mediumPerkDescriptions(const int perk)
{
    if (isAugmentedPerk(perk)) {
        QStringList levels[2];
        for (const TAugmentation& aug : Augmentations)
            if (aug.perk == perk)
                levels[aug.level - 1].append(augmentationDescription(aug.value, aug.spellId, aug.augType, 0.0, false, false));
        QString special;
        switch (perk) {
            case 26: special = u' ' + qtTrId("perk_description_AugSpecialSpells_special"); break;
            case 29: special = u' ' + qtTrId("perk_description_AugForkedSpells_special"); break;
            case 27: special = u' ' + qtTrId("perk_description_AugFocusSpells_special"); break;
            default: break;
        }
        return { levels[0].join(QStringLiteral("; ")) + special, levels[1].join(QStringLiteral("; ")) + special };
    }
    switch (perk) {
        case 7: return { qtTrId("perk_description_SkillBonusAxeClubSword") };
        case 8: return { qtTrId("perk_description_BattleInstinct").arg(6).arg(1).arg(5).arg(8) };
        case 9: return { qtTrId("perk_description_BattleHealing").arg(QStringLiteral("10")) };
        case 16: return { qtTrId("perk_description_BallisticMastery").arg(10).arg(4) };
        case 17: return { qtTrId("perk_description_PositionalTactics").arg(3).arg(3).arg(3) };
        case 23: return { qtTrId("perk_description_FocusMastery").arg(35).arg(12).arg(2) };
        case 28: return { qtTrId("perk_description_HealingLink").arg(25) };
        case 35: return { qtTrId("perk_description_RunicMastery").arg(25).arg(10).arg(20) };
        case 37: case 38: case 39: case 40: return { qtTrId("perk_description_VesselResonance") };
        case 41: return { qtTrId("perk_description_Sanctuary").arg(10).arg(2) };
        case 42: return { qtTrId("perk_description_GuidingPresence") };
        default: return {};
    }
}

QStringList withAugmentationIcons(QStringList lines, const int perk, const int count)
{
    if (!isAugmentedPerk(perk) || lines.size() < 2)
        return lines;
    lines[0].prepend(qtTrId(count < 1 ? "skill_wheel_dialog_augmentation1_inactive" : "skill_wheel_dialog_augmentation1_active"));
    lines[1].prepend(qtTrId(count < 2 ? "skill_wheel_dialog_augmentation2_inactive" : "skill_wheel_dialog_augmentation2_active"));
    return lines;
}

QString mediumPerkEffect(const int perk)
{
    const QString name = mediumPerkName(perk);
    QString prefix;
    switch (perk) {
        case 0: case 1: case 2: case 3: prefix = qtTrId("skill_wheel_dialog_perk_percent_bonus_effect_format").arg(2); break;
        case 5: prefix = qtTrId("skill_wheel_dialog_perk_percent_bonus_effect_format").arg(percentNumber(0.0025, 2, false)); break;
        case 6: prefix = qtTrId("skill_wheel_dialog_perk_percent_bonus_effect_format").arg(percentNumber(0.0075, 2, false)); break;
        case 7: case 15: case 34: case 43: prefix = QStringLiteral("+1"); break;
        default: break;
    }
    if (prefix.isEmpty())
        return name;
    return QStringLiteral("%1 %2").arg(prefix, name);
}

QString largePerkDetailedInformation(const int perk, const int stage)
{
    const auto cooldown = [](const int spellId) -> std::optional<int> {
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
        return spell ? std::optional<int>(spell->cooldownSelf) : std::nullopt;
    };
    const auto cooldownStages = [&](const int spellId, const int step) -> std::optional<QString> {
        const std::optional<int> seconds = cooldown(spellId);
        if (!seconds)
            return std::nullopt;
        return stageValues(*seconds, *seconds - step, *seconds - 2 * step, stage);
    };
    const auto percents = [&](const double a, const double b, const double c) {
        return stageValues(percentNumber(a, 2, false) + u'%', percentNumber(b, 2, false) + u'%', percentNumber(c, 2, false) + u'%', stage);
    };
    const auto wholePercents = [&](const double a, const double b, const double c) {
        return stageValues(wholeNumber(a) + u'%', wholeNumber(b) + u'%', wholeNumber(c) + u'%', stage);
    };
    const QString unknown = qtTrId("dummy_unknown");
    switch (perk) {
        case 0:
            return qtTrId("perk_description_GiftOfLife")
                .arg(stagePercents(20, 25, 30, stage))
                .arg(60)
                .arg(stageValues(compactMinutes(1800), compactMinutes(1200), compactMinutes(600), stage));
        case 1: {
            const auto cd = cooldownStages(0x105, 4);
            if (!cd)
                return unknown;
            return qtTrId("perk_description_ExecutionersThrow")
                .arg(stageValues(2, 3, 4, stage))
                .arg(stagePercents(100, 125, 150, stage))
                .arg(30)
                .arg(*cd);
        }
        case 2:
            return qtTrId("perk_description_CombatMastery").arg(stagePercents(14, 12, 10, stage)).arg(stagePercents(14, 12, 10, stage));
        case 3: case 6: case 9: case 12: case 15: {
            const std::optional<int> seconds = cooldown(0x108);
            if (!seconds)
                return unknown;
            const int minutes = *seconds / 60;
            return qtTrId("perk_description_AvatarOfVocation")
                .arg(15)
                .arg(stagePercents(5, 10, 15, stage))
                .arg(stagePercents(5, 10, 15, stage))
                .arg(stageValues(minutes, minutes - 30, minutes - 60, stage));
        }
        case 4: {
            const auto cd = cooldownStages(0x102, 6);
            return cd ? qtTrId("perk_description_DivineGrenade").arg(*cd) : unknown;
        }
        case 5: {
            const auto cd = cooldownStages(0x10c, 4);
            if (!cd)
                return unknown;
            return qtTrId("perk_description_DivineEmpowerment").arg(*cd).arg(stagePercents(8, 10, 12, stage));
        }
        case 7:
            if (!cooldown(0x104))
                return unknown;
            return qtTrId("perk_description_BeamMastery")
                .arg(wholePercents(25.0, 40.0, 70.0))
                .arg(1)
                .arg(3)
                .arg(wholePercents(10.0, 12.0, 14.0))
                .arg(wholePercents(30.0, 36.0, 42.0));
        case 8:
            return qtTrId("perk_description_LordOfDestruction")
                .arg(percents(0.019999999999999997, 0.030000000000000006, 0.04))
                .arg(percents(0.019999999999999997, 0.030000000000000006, 0.04))
                .arg(percents(0.15000000000000002, 0.25500000000000006, 0.3));
        case 10: {
            const auto cd = cooldownStages(0x106, 4);
            if (!cd)
                return unknown;
            return qtTrId("perk_description_TwinBursts").arg(stagePercents(20, 40, 60, stage)).arg(60).arg(*cd);
        }
        case 11:
            return qtTrId("perk_description_BlessingOfTheGrove")
                .arg(stageValues(trimmedPercentNumber(0.05000000074505806, 1) + u'%', trimmedPercentNumber(0.07500000298023224, 1) + u'%',
                                 trimmedPercentNumber(0.10000000149011612, 1) + u'%', stage));
        case 13: {
            const auto cd = cooldownStages(0x127, 4);
            if (!cd)
                return unknown;
            const auto g = [](const double v) { return QString::number(v, 'g', 6) + u'%'; };
            return qtTrId("perk_description_SpiritualOutburst").arg(*cd).arg(stageValues(g(37.5), g(50.0), g(62.5), stage));
        }
        case 14:
            return qtTrId("perk_description_Ascetic").arg(wholePercents(1.0, 2.0, 3.0)).arg(wholePercents(100.0, 200.0, 300.0));
        default:
            return unknown;
    }
}

QString perkLevelName(const int stage)
{
    switch (stage) {
        case 0: return qtTrId("skill_wheel_dialog_perk_level_0");
        case 1: return qtTrId("skill_wheel_dialog_perk_level_1");
        case 2: return qtTrId("skill_wheel_dialog_perk_level_2");
        case 3: return qtTrId("skill_wheel_dialog_perk_level_3");
        default: return qtTrId("dummy_unknown");
    }
}

QString domainName(const int domain) { return qtTrId(mediumPerkId(37 + std::clamp(domain, 0, 3))); }

QString basicStatName(const int stat, const bool multiString)
{
    if (stat >= 0 && stat < 9)
        return qtTrId("resistance_for_damge_type").arg(qtTrId(DamageTypeIds[stat]));
    switch (stat) {
        case 9: return multiString ? multi(qtTrId("mitigation_multiplier"), qtTrId("mitigation_multiplier_short")) : qtTrId("mitigation_multiplier");
        case 10: return qtTrId("hit_points");
        case 11: return qtTrId("mana");
        case 12: return qtTrId("capacity");
        default: return qtTrId("dummy_unknown");
    }
}

QString basicStatDamageTypeName(const int stat) { return stat >= 0 && stat < 9 ? qtTrId(DamageTypeIds[stat]) : qtTrId("dummy_unknown"); }

QString basicStatValueText(const double value, const int stat)
{
    if (stat >= 0 && stat < 9)
        return formatValue(std::lround(value * 100.0), 4);
    if (stat == 9)
        return formatValue(std::lround(value * 100.0), 6);
    if (stat >= 10 && stat <= 12)
        return formatValue(static_cast<int>(value), 1);
    return formatValue(0, 0);
}

QString basicStatInfo(const int stat) { return stat == 9 ? qtTrId("skill_wheel_dialog_mitigation_perk_info") : QString(); }

QString supremeModValueText(const double value, const int mod)
{
    if (mod < 0 || mod >= SupremeModCount)
        return formatValue(0, 0);
    const int category = SupremeCategories[mod];
    if (category < 4)
        return formatValue(static_cast<int>(value * 100.0), 4);
    if (category == 5)
        return formatValue(static_cast<int>(value), 1);
    const int augType = SupremeAugmentations[mod].augType;
    if (augType == 2 || augType == 3 || augType == 16 || augType == 17)
        return formatValue(static_cast<int>(value * 10000.0), 5);
    return formatValue(static_cast<int>(value), augType == 6 ? 8 : 7);
}

double TActiveMods::basicStat(const int stat) const
{
    const auto it = std::ranges::find(basicStats, stat, &std::pair<int, double>::first);
    return it != basicStats.end() ? it->second : 0.0;
}

double TActiveMods::supremeStat(const int mod) const
{
    const auto it = std::ranges::find(supremeStats, mod, &std::pair<int, double>::first);
    return it != supremeStats.end() ? it->second : 0.0;
}

namespace {

/// CIP 0x10111aba0: dodge, critical extra damage, life leech and mana leech by grade.
double supremeStatValue(const int mod, const int grade)
{
    const int g = std::clamp(grade, 0, 3);
    switch (mod) {
        case 0: return DodgeByGrade[g];
        case 1: return CriticalByGrade[g];
        case 2: return LifeLeechByGrade[g];
        case 3: return ManaLeechByGrade[g];
        default: return 0.0;
    }
}

/// CIP 0x10111d420: one stat of a gem's basic mods; the second mod needs resonance 2.
double gemBasicStat(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, const int stat, const int vocationIndex,
                    const int resonance)
{
    if (resonance == 0 || vocationIndex < 0 || vocationIndex >= VocationCount)
        return 0.0;
    const auto sum = [&](const int mod, const int grade) {
        double total = 0.0;
        if (mod < 0 || mod >= BasicModCount)
            return total;
        for (int i = 0; i < BasicMods[mod].count; ++i)
            if (BasicMods[mod].effects[i].stat == stat)
                total += BasicMods[mod].effects[i].values[vocationIndex][std::clamp(grade, 0, 3)];
        return total;
    };
    double total = sum(gem.basic1, effectiveGrade(gem, basicGrades, supremeGrades, 0));
    if (resonance != 1 && gem.quality >= static_cast<int>(EGemQuality::Regular))
        total += sum(gem.basic2, effectiveGrade(gem, basicGrades, supremeGrades, 1));
    return total;
}

}

TActiveMods activeMods(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& basicGrades, const TGrades& supremeGrades,
                       const int vocationIndex)
{
    std::array<const TGem*, DomainCount> socketed{};
    std::array<int, DomainCount> res{};
    for (int d = 0; d < DomainCount; ++d) {
        res[d] = resonance(wheel.points, d);
        if (wheel.gems[d] == NoGem)
            continue;
        const auto it = std::ranges::find_if(gems, [&](const TGem& g) { return g.id == wheel.gems[d]; });
        socketed[d] = it != gems.end() ? &*it : nullptr;
    }
    const auto inVessel = [&](const int d) { return socketed[d] && socketed[d]->domain == d; };
    const auto radiant = [&](const int d) { return res[d] == 3 && socketed[d]->quality >= static_cast<int>(EGemQuality::Greater); };

    TActiveMods mods;

    // CIP 0x10111b300: the stats of every socketed gem's basic mods, ascending.
    std::vector<int> stats;
    for (const TGem* gem : socketed) {
        if (!gem)
            continue;
        for (const int mod : { gem->basic1, gem->quality >= 1 ? gem->basic2 : -1 })
            if (mod >= 0 && mod < BasicModCount)
                for (int i = 0; i < BasicMods[mod].count; ++i)
                    stats.push_back(BasicMods[mod].effects[i].stat);
    }
    std::ranges::sort(stats);
    stats.erase(std::unique(stats.begin(), stats.end()), stats.end());
    for (const int stat : stats) {
        double value = 0.0;
        for (int d = 0; d < DomainCount; ++d)
            if (inVessel(d))
                value += gemBasicStat(*socketed[d], basicGrades, supremeGrades, stat, vocationIndex, res[d]);
        if (std::fabs(value) > 1e-5)
            mods.basicStats.emplace_back(stat, value);
    }

    // CIP 0x10111b730 / b9d0 / bc30 / be90.
    for (int mod = 0; mod < 4; ++mod) {
        double value = 0.0;
        for (int d = 0; d < DomainCount; ++d)
            if (inVessel(d) && radiant(d) && socketed[d]->supreme == mod)
                value += supremeStatValue(mod, effectiveGrade(*socketed[d], basicGrades, supremeGrades, 2));
        if (value != 0.0)
            mods.supremeStats.emplace_back(mod, value);
    }

    for (int d = 0; d < DomainCount; ++d) {
        if (!socketed[d] || !radiant(d))
            continue;
        const int mod = socketed[d]->supreme;
        if (mod < 0 || mod >= SupremeModCount || !SupremeAugmentations[mod].valid)
            continue;
        const TSupremeAugmentation& aug = SupremeAugmentations[mod];
        auto it = std::ranges::find(mods.augmentations, mod, &TActiveMods::TAugmentation::mod);
        if (it == mods.augmentations.end())
            it = mods.augmentations.insert(std::ranges::upper_bound(mods.augmentations, mod, {}, &TActiveMods::TAugmentation::mod),
                                           { mod, aug.spellId, aug.augType, 0.0, 0 });
        it->value += aug.values[std::clamp(effectiveGrade(*socketed[d], basicGrades, supremeGrades, 2), 0, 3)];
        ++it->count;
    }

    for (int d = 0; d < DomainCount; ++d) {
        if (!socketed[d] || !radiant(d))
            continue;
        const int perk = revelationMasteryPerk(socketed[d]->supreme);
        if (perk < 0 || std::ranges::find(mods.revelations, perk, &TActiveMods::TRevelation::perk) != mods.revelations.end())
            continue;
        // CIP 0x10111c3f0.
        int bonus = 0;
        for (int v = 0; v < DomainCount; ++v)
            if (inVessel(v) && radiant(v) && revelationMasteryPerk(socketed[v]->supreme) == perk)
                bonus += revelationMasteryBonus(effectiveGrade(*socketed[v], basicGrades, supremeGrades, 2));
        mods.revelations.push_back({ perk, bonus, socketed[d]->supreme });
    }

    for (int d = 0; d < DomainCount; ++d) {
        if (!inVessel(d))
            continue;
        const int quality = socketed[d]->quality;
        if (vesselMatchesGem(quality, res[d]))
            mods.damageHealing += quality >= static_cast<int>(EGemQuality::Greater) ? 2 : 1;
        const int mod = socketed[d]->supreme;
        if (radiant(d) && supremeModForVocation(mod, vocationIndex) && std::ranges::find(MomentumMods, mod) != std::end(MomentumMods))
            mods.momentum += MomentumByGrade[std::clamp(effectiveGrade(*socketed[d], basicGrades, supremeGrades, 2), 0, 3)];
    }
    return mods;
}

QString basicModEffect(const int mod, const int vocationIndex, const int grade)
{
    if (mod < 0 || mod >= BasicModCount || BasicMods[mod].count == 0)
        return qtTrId("dummy_unknown");
    const int voc = std::clamp(vocationIndex, 0, VocationCount - 1);
    const int g = std::clamp(grade, 0, 3);
    QStringList lines;
    for (int i = 0; i < BasicMods[mod].count; ++i) {
        const TBasicEffect& effect = BasicMods[mod].effects[i];
        const double v = effect.values[voc][g];
        qint64 value = 0;
        int format = 0;
        if (effect.stat >= 10 && effect.stat <= 12) {
            value = static_cast<int>(v);
            format = 1;
        } else {
            value = std::lround(v * 100.0);
            format = effect.stat == 9 ? 6 : 4;
        }
        lines << QStringLiteral("%1 %2").arg(formatModValue(value, format), basicStatName(effect.stat));
    }
    return lines.join(u'\n');
}

QString supremeModName(const int mod, const bool withPerk, const bool multiString)
{
    if (mod < 0 || mod >= SupremeModCount)
        return QString();
    switch (SupremeCategories[mod]) {
        case 0: return qtTrId("dodge");
        case 1: return qtTrId("skill_wheel_critical_extra_damage");
        case 2: return qtTrId("life_leech");
        case 3: return qtTrId("mana_leech");
        case 4: {
            const auto* spell = spells::TSpellInfoStorage::instance()->spell(SupremeAugmentations[mod].spellId);
            if (!spell)
                return qtTrId("dummy_unknown");
            const QString shortName = qtTrId("perk_name_prefix_augmentation_short").arg(spell->name);
            return multiString ? multi(qtTrId("perk_name_prefix_augmentation").arg(spell->name), shortName) : shortName;
        }
        case 5: {
            const QString label = qtTrId("skillwheel_gem_mod_revelation_mastery_label");
            if (!withPerk)
                return label;
            const QString perk = largePerkName(revelationMasteryPerk(mod));
            const QString longName = QStringLiteral("%1 %2").arg(label, perk);
            return multiString ? multi(longName, qtTrId("skillwheel_gem_mod_revelation_mastery_prefix_short").arg(perk)) : longName;
        }
        default: return QString();
    }
}

QString supremeModEffect(const int mod, const int grade)
{
    if (mod < 0 || mod >= SupremeModCount)
        return qtTrId("dummy_unknown");
    const int g = std::clamp(grade, 0, 3);
    const auto percent = [&](const double v) { return QStringLiteral("%1 %2").arg(formatModValue(static_cast<int>(v * 100.0), 4), supremeModName(mod, false, false)); };
    switch (SupremeCategories[mod]) {
        case 0: return percent(DodgeByGrade[g]);
        case 1: return percent(CriticalByGrade[g]);
        case 2: return percent(LifeLeechByGrade[g]);
        case 3: return percent(ManaLeechByGrade[g]);
        case 4: {
            const TSupremeAugmentation& aug = SupremeAugmentations[mod];
            const double v = aug.values[g];
            switch (aug.augType) {
                case 2: return qtTrId("augmentation_description_percentage_damage").arg(v * 100.0);
                case 3: return qtTrId("augmentation_description_percentage_healing").arg(v * 100.0);
                case 6: {
                    const bool momentum = std::ranges::find(MomentumMods, mod) != std::end(MomentumMods);
                    if (momentum && MomentumByGrade[g] > 0.0)
                        return qtTrId("augmentation_description_cooldown_with_momentum").arg(v).arg(MomentumByGrade[g]);
                    return qtTrId("augmentation_description_cooldown").arg(v);
                }
                case 16: return qtTrId("augmentation_description_critical_damage_short").arg(v * 100.0);
                default: return qtTrId("dummy_unknown");
            }
        }
        case 5: return qtTrId("skillwheel_gem_mod_revelation_mastery").arg(largePerkName(revelationMasteryPerk(mod))).arg(revelationMasteryBonus(g));
        default: return qtTrId("dummy_unknown");
    }
}

int revelationMasteryPerk(const int mod)
{
    for (const auto& [m, perk] : RevelationMasteryPerks)
        if (m == mod)
            return perk;
    return -1;
}

int revelationMasteryBonus(const int grade) { return RevelationMasteryBonus[std::clamp(grade, 0, 3)]; }

QUrl imageUrl(const char* provider, const int index) { return QUrl(QStringLiteral("image://%1/%2").arg(QLatin1String(provider)).arg(index)); }

QUrl gemImage(const TGem& gem, const int vocationIndex)
{
    return imageUrl("icons-skillwheel-gemvariants", gem.quality + std::clamp(vocationIndex, 0, VocationCount - 1) * 12 + gem.domain * 3);
}

QUrl socketBezel(const int domain, const int resonance, const int quality, const bool hasGem)
{
    if (resonance <= 0)
        return {};
    const bool match = hasGem && quality + 1 == resonance;
    return imageUrl("icons-skillwheel-sockets", resonance + domain * 3 + (match ? 0 : 12));
}

QUrl domainAffinityImage(const int domain) { return imageUrl("icons-skillwheel-gem-domainaffinity", domain); }
QUrl basicModImage(const int mod) { return imageUrl("icons-skillwheel-gem-basicmods", mod); }
QUrl supremeModImage(const int mod) { return imageUrl("icons-skillwheel-gem-suprememods", mod); }

QString modGradeImage(const int grade) { return imageUrl("backdrop-mod-grade", grade).toString(); }
QString modGradePotentialImage(const int grade) { return imageUrl("overlay-mod-grade-potential", grade).toString(); }
QString modGradeHighlightImage(const int grade) { return imageUrl("backdrop-mod-grade", grade + 4).toString(); }

int fragmentCost(const int grade)
{
    constexpr int Costs[] = { 5, 15, 30 };
    return grade >= 0 && grade < 3 ? Costs[grade] : 0;
}

int basicGoldCost(const int grade)
{
    constexpr int Costs[] = { 2000000, 5000000, 30000000 };
    return grade >= 0 && grade < 3 ? Costs[grade] : 0;
}

int supremeGoldCost(const int grade)
{
    constexpr int Costs[] = { 5000000, 12500000, 75000000 };
    return grade >= 0 && grade < 3 ? Costs[grade] : 0;
}

int revealCost(const int quality)
{
    constexpr int Costs[] = { 125000, 1000000, 6000000 };
    return quality >= 0 && quality < 3 ? Costs[quality] : 0;
}

int changeAffinityCost(const int quality)
{
    constexpr int Costs[] = { 125000, 250000, 500000 };
    return quality >= 0 && quality < 3 ? Costs[quality] : 0;
}

QString romanNumeral(int value)
{
    constexpr std::pair<int, const char*> Numerals[] = {
        { 1000, "M" }, { 900, "CM" }, { 500, "D" }, { 400, "CD" }, { 100, "C" }, { 90, "XC" }, { 50, "L" },
        { 40, "XL" }, { 10, "X" }, { 9, "IX" }, { 5, "V" }, { 4, "IV" }, { 1, "I" },
    };
    QString text;
    for (const auto& [n, numeral] : Numerals)
        for (; value >= n; value -= n)
            text += QLatin1String(numeral);
    return text;
}

QString formatValue(const qint64 value, const int format)
{
    const QLocale english(QLocale::English);
    switch (format) {
        case 0: return QString();
        case 1: return sign(static_cast<double>(value)) + englishNumber(value);
        case 2: return qtTrId("skill_wheel_dialog_perk_percent_bonus_effect_format").arg(static_cast<int>(value));
        case 3: return qtTrId("skill_wheel_dialog_perk_percent_bonus_effect_format").arg(english.toString(static_cast<double>(value) * 0.01, 'f', 2));
        case 4:
        case 5: return formatModValue(value, 4);
        case 6: return formatModValue(value, 6);
        case 7: return romanNumeral(static_cast<int>(value));
        case 8: return QStringLiteral("%1s").arg(value);
        default: return qtTrId("dummy_unknown");
    }
}

qint64 smallPerkTotal(const std::array<int, SlotCount>& points, const ESmallPerk perk, const int vocationIndex)
{
    int sum = 0;
    for (int slot = 0; slot < SlotCount; ++slot) {
        const ESmallPerk p = smallPerk(slot);
        if (p == perk || (p == ESmallPerk::LifeAndMana && (perk == ESmallPerk::Life || perk == ESmallPerk::Mana)))
            sum += points[slot];
    }
    if (perk == ESmallPerk::Mitigation)
        return static_cast<qint64>(static_cast<float>(sum) * MitigationPerPoint * 100.0f);
    return smallPerkValue(perk, sum, vocationIndex);
}

int smallPerkFormat(const ESmallPerk perk) { return perk == ESmallPerk::Mitigation ? 6 : 1; }

int largePerkExtraDamageHealing(const int stage)
{
    constexpr int Bonus[] = { 0, 4, 9, 20 };
    return Bonus[std::clamp(stage, 0, 3)];
}

int basicModCount() { return BasicModCount; }
int supremeModCount() { return SupremeModCount; }

bool hasBasicMod(const int mod) { return mod >= 0 && mod < BasicModCount && BasicMods[mod].count > 0; }

bool isAugmentationMod(const int mod) { return mod >= 0 && mod < SupremeModCount && SupremeCategories[mod] == 4; }

bool supremeModForVocation(const int mod, const int vocationIndex)
{
    if ((mod >= 0 && mod <= 3) || mod == 5)
        return true;
    if (mod == 4)
        return vocationIndex == 2 || vocationIndex == 3;
    if (mod >= 0x06 && mod <= 0x17)
        return vocationIndex == 0;
    if (mod >= 0x18 && mod <= 0x29)
        return vocationIndex == 1;
    if (mod >= 0x2a && mod <= 0x3a)
        return vocationIndex == 2;
    if (mod >= 0x3b && mod <= 0x4b)
        return vocationIndex == 3;
    if (mod >= 0x4c && mod <= 0x5d)
        return vocationIndex == 4;
    return false;
}

int totalModGradePoints(const int vocationIndex)
{
    const int basic = static_cast<int>(std::ranges::count_if(BasicMods, [](const TBasicMod& m) { return m.count > 0; }));
    return (vocationIndex >= 0 && vocationIndex < VocationCount ? 23 : 0) + basic;
}

int modGradePoints(const TGrades& basicGrades, const TGrades& supremeGrades)
{
    int points = 0;
    for (const TGrades* grades : { &basicGrades, &supremeGrades })
        for (const auto& [mod, grade] : *grades)
            points += grade == 3 ? 1 : 0;
    return points;
}

QString vesselActivation(const int resonance, const bool plural)
{
    const QString suffix = plural ? QStringLiteral("s") : QString();
    switch (resonance) {
        case 0: return qtTrId("skill_wheel_dialog_vessel_activation_closed").arg(suffix);
        case 1: return qtTrId("skill_wheel_dialog_vessel_activation_one_mod").arg(suffix);
        case 2: return qtTrId("skill_wheel_dialog_vessel_activation_two_mod").arg(suffix);
        case 3: return qtTrId("skill_wheel_dialog_vessel_activation_three_mod").arg(suffix);
        default: return qtTrId("dummy_unknown");
    }
}

bool vesselMatchesGem(const int quality, const int resonance)
{
    return (quality == 0 && resonance == 1) || (quality == 1 && resonance == 2) || (quality == 2 && resonance == 3);
}

QString vesselBonusInformation()
{
    return qtTrId("skill_wheel_dialog_vessel_matches_gem_bonus_information")
        .arg(1)
        .arg(1)
        .arg(vesselActivation(1, true), vesselActivation(2, true), vesselActivation(3, true),
             qtTrId("skill_wheel_atelier_gem_quality_filter_lesser"), qtTrId("skill_wheel_atelier_gem_quality_filter_regular"),
             qtTrId("skill_wheel_atelier_gem_quality_filter_greater"));
}

QString gemInformation()
{
    return qtTrId("skill_wheel_dialog_gem_information")
        .arg(qtTrId("skill_wheel_atelier_gem_quality_filter_lesser"), qtTrId("skill_wheel_atelier_gem_quality_filter_regular"),
             qtTrId("skill_wheel_atelier_gem_quality_filter_greater"));
}

int unrevealedGemTypeId(const int vocationIndex, const int quality)
{
    constexpr int Lesser[VocationCount] = { 44602, 44605, 44608, 44611, 49371 };
    if (vocationIndex < 0 || vocationIndex >= VocationCount || quality < 0 || quality > 2)
        return 0;
    return Lesser[vocationIndex] + quality;
}

namespace {

constexpr int CodeSize = SlotCount + DomainCount * 3;
constexpr uint8_t CodeNone = 0xff;
/// CIP 0x101126f80: basic mods below 49, supreme mods below 94.
constexpr int CodeBasicLimit = 0x31;
constexpr int CodeSupremeLimit = 0x5e;
/// CIP 0x10232ea80 (export) and 0x1011281e0 (import), by vocation index.
constexpr char CodeLetters[VocationCount] = { 'K', 'P', 'S', 'D', 'M' };
constexpr const char* LegacyCodePrefixes[VocationCount] = { "EK", "RP", "MS", "ED", "EM" };

/// CIP 0x101128f90 / 0x101129b70: either base64 alphabet, `=` or `.` padding.
std::optional<QByteArray> fromCodeBase64(QByteArray text)
{
    text.replace('+', '-').replace('/', '_').replace('.', '=');
    while (text.size() % 4 != 0)
        text.append('=');
    auto result = QByteArray::fromBase64Encoding(text, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!result)
        return std::nullopt;
    return result.decoded;
}

}

QString encodeWheelCode(const TWheel& wheel, const std::vector<TGem>& gems, const int vocationIndex)
{
    if (vocationIndex < 0 || vocationIndex >= VocationCount)
        return {};
    std::array<uint8_t, CodeSize> raw{};
    for (int s = 0; s < SlotCount; ++s)
        raw[s] = static_cast<uint8_t>(std::clamp(wheel.points[s], 0, 0xff));
    for (int d = 0; d < DomainCount; ++d) {
        uint8_t* bytes = raw.data() + SlotCount + d * 3;
        bytes[0] = bytes[1] = bytes[2] = CodeNone;
        const auto it = std::ranges::find_if(gems, [&](const TGem& g) { return g.id == wheel.gems[d]; });
        if (wheel.gems[d] == NoGem || it == gems.end())
            continue;
        bytes[0] = static_cast<uint8_t>(it->basic1);
        if (it->quality >= static_cast<int>(EGemQuality::Regular))
            bytes[1] = static_cast<uint8_t>(it->basic2);
        if (it->quality >= static_cast<int>(EGemQuality::Greater))
            bytes[2] = static_cast<uint8_t>(it->supreme);
    }

    z_stream stream{};
    if (deflateInit2(&stream, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray packed(static_cast<qsizetype>(deflateBound(&stream, CodeSize)), '\0');
    stream.next_in = raw.data();
    stream.avail_in = CodeSize;
    stream.next_out = reinterpret_cast<Bytef*>(packed.data());
    stream.avail_out = static_cast<uInt>(packed.size());
    const int result = deflate(&stream, Z_FINISH);
    packed.resize(static_cast<qsizetype>(stream.total_out));
    deflateEnd(&stream);
    if (result != Z_STREAM_END)
        return {};
    return QString(QChar(CodeLetters[vocationIndex])) + QLatin1Char('0')
        + QString::fromLatin1(packed.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

std::optional<TWheelCode> decodeWheelCode(const QString& code)
{
    const QByteArray text = code.toLatin1();
    TWheelCode result;
    for (int v = 0; v < VocationCount; ++v) {
        if (!text.startsWith(LegacyCodePrefixes[v]))
            continue;
        if (text.size() != 2 + SlotCount * 4 / 3)
            return std::nullopt;
        const auto points = fromCodeBase64(text.mid(2).replace('\n', QByteArray()));
        if (!points)
            return std::nullopt;
        result.vocationIndex = v;
        for (qsizetype s = 0; s < points->size() && s < SlotCount; ++s)
            result.points[s] = static_cast<uint8_t>(points->at(s));
        return result;
    }

    if (text.size() < 2 || text.at(1) != '0')
        return std::nullopt;
    const char* letter = std::ranges::find(CodeLetters, text.front());
    if (letter == std::end(CodeLetters))
        return std::nullopt;
    const auto packed = fromCodeBase64(text.mid(2));
    if (!packed)
        return std::nullopt;
    std::array<uint8_t, CodeSize> raw{};
    z_stream stream{};
    if (inflateInit2(&stream, -15) != Z_OK)
        return std::nullopt;
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(packed->constData()));
    stream.avail_in = static_cast<uInt>(packed->size());
    stream.next_out = raw.data();
    stream.avail_out = CodeSize;
    const int inflated = inflate(&stream, Z_FINISH);
    const uInt unread = stream.avail_in;
    inflateEnd(&stream);
    if (inflated != Z_STREAM_END || unread != 0)
        return std::nullopt;

    result.vocationIndex = static_cast<int>(letter - std::begin(CodeLetters));
    for (int s = 0; s < SlotCount; ++s)
        result.points[s] = raw[s];
    for (int d = 0; d < DomainCount; ++d) {
        const uint8_t* bytes = raw.data() + SlotCount + d * 3;
        if (bytes[0] >= CodeBasicLimit || (bytes[1] != CodeNone && bytes[1] >= CodeBasicLimit)
            || (bytes[2] != CodeNone && (bytes[2] >= CodeSupremeLimit || bytes[1] == CodeNone)))
            continue;
        result.gems[d] = { bytes[0], bytes[1] == CodeNone ? -1 : bytes[1], bytes[2] == CodeNone ? -1 : bytes[2] };
    }
    return result;
}

bool isValidWheelCode(const TWheelCode& code) { return isValid(code.points); }

}
