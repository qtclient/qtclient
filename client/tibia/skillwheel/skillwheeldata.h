#pragma once

#include <QString>
#include <QUrl>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia::skillwheel {

constexpr int SlotCount = 36;
constexpr int DomainCount = 4;
constexpr int VocationCount = 5;
constexpr int NoGem = -1;
/// CIP 0x100e018f0.
constexpr int MaxRevealedGems = 600;
/// CIP 0x100de1520: at most 10 presets; 0x100de1570: the last one cannot be deleted.
constexpr int MaxPresets = 10;
/// CIP 0x100ddb940 (action 307/308): the level the wheel needs.
constexpr int RequiredLevel = 51;

/// Small (dedication) perk types: life, mana, life and mana, capacity, mitigation.
enum class ESmallPerk { Life = 0, Mana = 1, LifeAndMana = 2, Capacity = 3, Mitigation = 4 };
/// Gem quality: the number of mods (lesser 1, regular 2, greater 3).
enum class EGemQuality { Lesser = 0, Regular = 1, Greater = 2 };

/// A revealed gem (Crystal addGems: the id is the gem's index in the revealed list).
struct TGem
{
    int id = 0;
    bool locked = false;
    int domain = 0;
    int quality = 0;
    int basic1 = 0;
    int basic2 = 0;
    int supreme = 0;
};

/// The points of the 36 slices and the gem id socketed in each domain's vessel.
struct TWheel
{
    std::array<int, SlotCount> points{};
    std::array<int, DomainCount> gems{ NoGem, NoGem, NoGem, NoGem };

    bool operator==(const TWheel&) const = default;
};

/// Mod grades (0..3) by mod id, as the server sends them.
using TGrades = std::map<int, int>;

int maxPoints(int slot);
ESmallPerk smallPerk(int slot);
/// The conviction perk of a slice for a vocation index (0 knight .. 4 monk).
/// Perks 37..40 are the vessels of domain 0..3.
int mediumPerk(int slot, int vocationIndex);
bool isVesselPerk(int perk);
/// CIP mask 0x1f013ef7c7c00: the conviction perks that augment spells.
bool isAugmentedPerk(int perk);
/// The slices of a domain, from the centre out (CIP quadrant tables).
const std::array<int, 9>& domainSlots(int domain);
int domainOfSlot(int slot);
/// The revelation perk of a domain for a vocation index.
int largePerk(int domain, int vocationIndex);

/// CIP grid rules: every slice with points is a centre slice or reaches one
/// through full slices, and no slice exceeds its maximum.
bool isValid(const std::array<int, SlotCount>& points);
/// The slice can receive points: it has points, or filling it keeps the grid valid.
bool isUnlocked(const std::array<int, SlotCount>& points, int slot);
/// Points can be taken from the slice without breaking a path.
bool canDecrease(const std::array<int, SlotCount>& points, int slot);

/// Full vessel slices in a domain (vessel resonance, 0..3).
int resonance(const std::array<int, SlotCount>& points, int domain);
/// The revelation points of a domain: its slices plus the gem's revelation mastery.
int revelationPoints(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& supremeGrades, int domain, int vocationIndex);
/// 0 (locked) .. 3 for the revelation thresholds 250 / 500 / 1000.
int revelationStage(int points);
int revelationThreshold(int stage);

/// Mod effect grade: a mod's grade is capped by the grades of the mods before it (CIP 0x10111a7a0).
int effectiveGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, int modIndex);
int modCount(const TGem& gem);
bool isSupremeModIndex(int modIndex);
int modId(const TGem& gem, int modIndex);
int gemGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, int modIndex);

/// English locale number text.
QString englishNumber(qint64 value);

/// CIP 0x100fa9aa0: the small perk name (mitigation: long and short multi string).
QString smallPerkName(ESmallPerk perk);
/// CIP 0x100fa9e50: "+N Hit Points", "+N Mana", "+N Capacity", "N% Mitigation Multiplier".
QString smallPerkValueText(ESmallPerk perk, int points, int vocationIndex);
/// CIP 0x100faa700: the per-point information of a small perk.
QString smallPerkInfo(ESmallPerk perk, int vocationIndex);
int smallPerkValue(ESmallPerk perk, int points, int vocationIndex);
double mitigationValue(int points);

/// The part of a CIP multi string (long, short), or the first part.
QString multiPart(const QString& text, int index);
/// CIP 0x100faaa60: a multi string for augmentations and vessels.
QString mediumPerkName(int perk);
/// CIP 0x1011228a0: the full slices with a conviction perk, over all domains.
int mediumPerkCount(const std::array<int, SlotCount>& points, int perk, int vocationIndex);
/// CIP 0x100fabdb0 (value 0x10111d860): the summed effect of a conviction perk.
QString mediumPerkValueText(const std::array<int, SlotCount>& points, int perk, int vocationIndex);
/// CIP 0x100fabf50: the description lines of a conviction perk; an augmented
/// spell gets its two levels (the start-up table 0x10110ec70).
QStringList mediumPerkDescriptions(int perk);
/// CIP 0x100df5a90: the augmentation level icons before an augmented spell's
/// two lines, active for the levels `count` full slices unlock.
QStringList withAugmentationIcons(QStringList lines, int perk, int count);
/// CIP 0x100fae330: a conviction perk with its single-slice effect ("+1 Sword Fighting").
QString mediumPerkEffect(int perk);
/// CIP 0x100fa2850: one augmentation line of a spell.
QString augmentationDescription(double value, int spellId, int augType, double momentum, bool hasMomentum, bool shortForm);
/// CIP 0x100fa3480.
QString largePerkName(int perk);
/// CIP 0x100fa9240.
QString largePerkShortInformation(int perk);
/// CIP 0x100fa3710: the detailed text of a large perk, the values of the
/// reached revelation stage highlighted.
QString largePerkDetailedInformation(int perk, int stage);
/// CIP 0x100fa9460.
QString perkLevelName(int stage);
QString domainName(int domain);

/// CIP 0x100faf6a0: the text of every effect of a basic mod ("+1% Physical Resistance").
QString basicModEffect(int mod, int vocationIndex, int grade);
/// CIP 0x100faeff0: a stat name of the basic mods (`multiString`: mitigation long and short).
QString basicStatName(int stat, bool multiString = false);
/// CIP 0x100faf4d0: the damage type name of a resistance stat (0..8).
QString basicStatDamageTypeName(int stat);
/// CIP 0x100faf5b0: a basic stat total (resistances and mitigation in percent, else a number).
QString basicStatValueText(double value, int stat);
/// CIP 0x100faf520: the mitigation information, empty for the other stats.
QString basicStatInfo(int stat);
/// CIP 0x100fb0820 / 0x100fb0730: a supreme mod total in its display format.
QString supremeModValueText(double value, int mod);

/// CIP 0x101124700: the effects of the active mods of the socketed gems.
struct TActiveMods
{
    struct TAugmentation
    {
        int mod = 0;
        int spellId = 0;
        int augType = 0;
        double value = 0.0;
        int count = 0;
    };
    struct TRevelation
    {
        int perk = 0;
        int bonus = 0;
        int mod = 0;
    };

    /// Basic stats (0..8 resistances, 9 mitigation, 10 life, 11 mana, 12 capacity) by id, |value| > 1e-5.
    std::vector<std::pair<int, double>> basicStats;
    /// Supreme mods 0..3 (dodge, critical extra damage, life leech, mana leech) with a value.
    std::vector<std::pair<int, double>> supremeStats;
    /// Spell augmentations of the radiant vessels by mod id.
    std::vector<TAugmentation> augmentations;
    std::vector<TRevelation> revelations;
    /// CIP 0x10111cac0: the vessels whose resonance matches the gem quality.
    int damageHealing = 0;
    /// CIP 0x10111c0f0.
    double momentum = 0.0;

    double basicStat(int stat) const;
    double supremeStat(int mod) const;
};
TActiveMods activeMods(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& basicGrades, const TGrades& supremeGrades,
                       int vocationIndex);
/// CIP 0x100fafb90: `withPerk` adds the revelation perk, `multiString` the long form.
QString supremeModName(int mod, bool withPerk, bool multiString);
/// CIP 0x100fb0860.
QString supremeModEffect(int mod, int grade);
/// The large perk a revelation mastery mod raises (-1 for other mods).
int revelationMasteryPerk(int mod);
int revelationMasteryBonus(int grade);

QUrl imageUrl(const char* provider, int index);
/// CIP gem variant image: quality + vocation * 12 + domain * 3.
QUrl gemImage(const TGem& gem, int vocationIndex);
/// The vessel bezel of a domain for a resonance and the socketed gem quality.
QUrl socketBezel(int domain, int resonance, int quality, bool hasGem);
QUrl domainAffinityImage(int domain);
QUrl basicModImage(int mod);
QUrl supremeModImage(int mod);
/// CIP 0x100926de0 / 0x100926ea0 / 0x100926f60.
QString modGradeImage(int grade);
QString modGradePotentialImage(int grade);
QString modGradeHighlightImage(int grade);

/// Fragment workshop costs (CIP): fragments 5 / 15 / 30; gold basic 2M / 5M / 30M, supreme 5M / 12.5M / 75M.
int fragmentCost(int grade);
int basicGoldCost(int grade);
int supremeGoldCost(int grade);
/// CIP 0x100e019b0 .. 0x100e019d0: reveal costs; 0x1022e8c94: domain switch costs.
int revealCost(int quality);
int changeAffinityCost(int quality);

/// CIP 0x100fa2360: a perk value in a display format (0 none, 1 number,
/// 2/3 "+N%", 4/5 signed percent of value / 100, 6 fixed percent, 7 Roman, 8 seconds).
QString formatValue(qint64 value, int format);
/// CIP 0x100df8c40 style Roman numerals.
QString romanNumeral(int value);
/// The summed small perk of the whole grid (life and mana count the life-and-mana slices too).
qint64 smallPerkTotal(const std::array<int, SlotCount>& points, ESmallPerk perk, int vocationIndex);
/// The display format of a small perk total (mitigation 6, else 1).
int smallPerkFormat(ESmallPerk perk);

/// CIP 0x100fa1430 summary: the damage and healing bonus of a revelation stage (+4 / +9 / +20).
int largePerkExtraDamageHealing(int stage);

int basicModCount();
int supremeModCount();
/// The basic mod exists (mod 32 is an empty slot of the table).
bool hasBasicMod(int mod);
/// CIP 0x100fb0690: the supreme mod is a spell augmentation.
bool isAugmentationMod(int mod);
/// CIP mod list (0x101122xxx): supreme mods 0..3 and 5 suit every vocation, the rest one vocation index.
bool supremeModForVocation(int mod, int vocationIndex);
/// CIP 0x10111aa80: the grade points a vocation can earn (23 supreme + the basic mods).
int totalModGradePoints(int vocationIndex);
/// CIP 0x10111a950: the mods at the highest grade, one promotion point each.
int modGradePoints(const TGrades& basicGrades, const TGrades& supremeGrades);

/// CIP 0x100fb0cd0: "Sealed / Dormant / Awakened / Radiant Vessel" for a resonance.
QString vesselActivation(int resonance, bool plural);
/// CIP 0x100fb0f20: the vessel resonance matches the gem quality.
bool vesselMatchesGem(int quality, int resonance);
/// CIP 0x100fb0f50.
QString vesselBonusInformation();
/// CIP 0x100fb15b0.
QString gemInformation();

/// The unrevealed gem object of a vocation index and quality (Crystal vocations.xml).
int unrevealedGemTypeId(int vocationIndex, int quality);

/// The mods of a socketed gem in an export code (CIP 0x10111b190); -1 none.
struct TCodeGem
{
    int basic1 = -1;
    int basic2 = -1;
    int supreme = -1;
};

/// A decoded wheel export code.
struct TWheelCode
{
    int vocationIndex = 0;
    std::array<int, SlotCount> points{};
    std::array<TCodeGem, DomainCount> gems{};
};

/// CIP 0x1011274d0: `<K|P|S|D|M>0` and the url base64 (no padding) of the raw
/// deflated 36 slice points and 4 vessel gems.
QString encodeWheelCode(const TWheel& wheel, const std::vector<TGem>& gems, int vocationIndex);
/// CIP 0x1011281e0: that format, or the legacy `EK|RP|MS|ED|EM` + 48 base64
/// characters of the slice points; empty on any decoding error.
std::optional<TWheelCode> decodeWheelCode(const QString& code);
/// CIP 0x101125c70: the slices respect their maximum and paths.
bool isValidWheelCode(const TWheelCode& code);

}
