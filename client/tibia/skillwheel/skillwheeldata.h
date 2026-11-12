#pragma once

#include <QString>
#include <QUrl>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace skillwheel {

constexpr int SlotCount = 36;
constexpr int DomainCount = 4;
/// CipSoft External: 8 quality-typed vessels (2 lesser, 2 regular, 4 greater).
constexpr int VesselCount = 8;
constexpr int VocationCount = 5;
constexpr int NoGem = -1;
/// Medium perk ids (External jump table): Gem Vessel + Revelation Mastery.
constexpr int VesselNodePerk = 37;
constexpr int MasteryIncreasePerk = 46;
/// Vessel socket qualities by unlock index (wiki / CipSoft).
constexpr std::array<int, VesselCount> VesselQualities = { 0, 0, 1, 1, 2, 2, 2, 2 };
/// Promotion points granted by each filled MasteryIncrease slice.
constexpr int MasteryIncreasePoints = 125;
/// Fragment workshop: +5 Hit Points / Mana / Capacity per grade step.
constexpr int BonusBaseStatsPerGrade = 5;
/// CIP .
constexpr int MaxRevealedGems = 600;
/// CIP : at most 10 presets; : the last one cannot be deleted.
constexpr int MaxPresets = 10;
/// CIP (action 307/308): the level the wheel needs.
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

/// The points of the 36 slices and the gem id socketed in each vessel (0..7).
struct TWheel
{
    std::array<int, SlotCount> points{};
    std::array<int, VesselCount> gems{
        NoGem, NoGem, NoGem, NoGem, NoGem, NoGem, NoGem, NoGem,
    };

    bool operator==(const TWheel&) const = default;
};

/// Mod grades (0..3) by mod id, as the server sends them.
using TGrades = std::map<int, int>;

int maxPoints(int slot);
ESmallPerk smallPerk(int slot);
/// The conviction perk of a slice for a vocation index (0 knight .. 4 monk).
/// External: VesselNode (37) unlocks gem vessels; MasteryIncrease (46) adds revelation.
int mediumPerk(int slot, int vocationIndex);
bool isVesselPerk(int perk);
bool isMasteryIncreasePerk(int perk);
/// Fixed vessel quality for atelier socket index 0..7.
int vesselQualityAt(int vesselIndex);
/// How many Gem Vessel slices are filled (unlocks that many sockets).
int unlockedVesselCount(const std::array<int, SlotCount>& points, int vocationIndex);
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

/// Legacy vessel-resonance count (always 0 after CipSoft Winter 2026).
int resonance(const std::array<int, SlotCount>& points, int domain);
/// Revelation points: domain slices + MasteryIncrease (+125) + supreme revelation mastery gems.
int revelationPoints(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& supremeGrades, int domain, int vocationIndex);
/// Fragment workshop totals from all mod grades.
int bonusBaseStats(const TGrades& basicGrades, const TGrades& supremeGrades, int vocationIndex);
int bonusPromotionPoints(const TGrades& basicGrades, const TGrades& supremeGrades, int vocationIndex);
/// 0 (locked) .. 3 for the revelation thresholds 250 / 500 / 1000.
int revelationStage(int points);
int revelationThreshold(int stage);

/// Mod effect grade: CipSoft Winter 2026 — each mod uses its own grade (highest wins).
int effectiveGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, int modIndex);
int modCount(const TGem& gem);
bool isSupremeModIndex(int modIndex);
int modId(const TGem& gem, int modIndex);
int gemGrade(const TGem& gem, const TGrades& basicGrades, const TGrades& supremeGrades, int modIndex);

/// English locale number text.
QString englishNumber(qint64 value);

/// CIP : the small perk name (mitigation: long and short multi string).
QString smallPerkName(ESmallPerk perk);
/// CIP : "+N Hit Points", "+N Mana", "+N Capacity", "N% Mitigation Multiplier".
QString smallPerkValueText(ESmallPerk perk, int points, int vocationIndex);
/// CIP : the per-point information of a small perk.
QString smallPerkInfo(ESmallPerk perk, int vocationIndex);
int smallPerkValue(ESmallPerk perk, int points, int vocationIndex);
double mitigationValue(int points);

/// The part of a CIP multi string (long, short), or the first part.
QString multiPart(const QString& text, int index);
/// CIP : a multi string for augmentations and vessels.
QString mediumPerkName(int perk);
/// CIP : the full slices with a conviction perk, over all domains.
int mediumPerkCount(const std::array<int, SlotCount>& points, int perk, int vocationIndex);
/// CIP (value): the summed effect of a conviction perk.
QString mediumPerkValueText(const std::array<int, SlotCount>& points, int perk, int vocationIndex);
/// CIP : the description lines of a conviction perk; an augmented
/// spell gets its two levels (the start-up table).
QStringList mediumPerkDescriptions(int perk);
/// CIP : the augmentation level icons before an augmented spell's
/// two lines, active for the levels `count` full slices unlock.
QStringList withAugmentationIcons(QStringList lines, int perk, int count);
/// CIP : a conviction perk with its single-slice effect ("+1 Sword Fighting").
QString mediumPerkEffect(int perk);
/// CIP : one augmentation line of a spell.
QString augmentationDescription(double value, int spellId, int augType, double momentum, bool hasMomentum, bool shortForm);
/// CIP .
QString largePerkName(int perk);
/// CIP .
QString largePerkShortInformation(int perk);
/// CIP : the detailed text of a large perk, the values of the
/// reached revelation stage highlighted.
QString largePerkDetailedInformation(int perk, int stage);
/// CIP .
QString perkLevelName(int stage);
QString domainName(int domain);

/// CIP : the text of every effect of a basic mod ("+1% Physical Resistance").
QString basicModEffect(int mod, int vocationIndex, int grade);
/// CIP : a stat name of the basic mods (`multiString`: mitigation long and short).
QString basicStatName(int stat, bool multiString = false);
/// CIP : the damage type name of a resistance stat (0..8).
QString basicStatDamageTypeName(int stat);
/// CIP : a basic stat total (resistances and mitigation in percent, else a number).
QString basicStatValueText(double value, int stat);
/// CIP : the mitigation information, empty for the other stats.
QString basicStatInfo(int stat);
/// CIP / : a supreme mod total in its display format.
QString supremeModValueText(double value, int mod);

/// CIP : the effects of the active mods of the socketed gems.
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
    /// CIP : the vessels whose resonance matches the gem quality.
    int damageHealing = 0;
    /// CIP .
    double momentum = 0.0;

    double basicStat(int stat) const;
    double supremeStat(int mod) const;
};
TActiveMods activeMods(const TWheel& wheel, const std::vector<TGem>& gems, const TGrades& basicGrades, const TGrades& supremeGrades,
                       int vocationIndex);
/// CIP : `withPerk` adds the revelation perk, `multiString` the long form.
QString supremeModName(int mod, bool withPerk, bool multiString);
/// CIP .
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
/// CIP /.
QString modGradeImage(int grade);
QString modGradePotentialImage(int grade);
QString modGradeHighlightImage(int grade);

/// Fragment workshop costs (CIP): fragments 5 / 15 / 30; gold basic 2M / 5M / 30M, supreme 5M / 12.5M / 75M.
int fragmentCost(int grade);
int basicGoldCost(int grade);
int supremeGoldCost(int grade);
/// CIP .. : reveal costs; : domain switch costs.
int revealCost(int quality);
int changeAffinityCost(int quality);
/// CipSoft External reshapeGemCost : gold {125k,1M,5M}; fragments quality+1.
int reshapeGemCost(int quality, bool goldCost);

/// CIP : a perk value in a display format (0 none, 1 number,
/// 2/3 "+N%", 4/5 signed percent of value / 100, 6 fixed percent, 7 Roman, 8 seconds).
QString formatValue(qint64 value, int format);
/// CIP style Roman numerals.
QString romanNumeral(int value);
/// The summed small perk of the whole grid (life and mana count the life-and-mana slices too).
qint64 smallPerkTotal(const std::array<int, SlotCount>& points, ESmallPerk perk, int vocationIndex);
/// The display format of a small perk total (mitigation 6, else 1).
int smallPerkFormat(ESmallPerk perk);

/// CIP summary: the damage and healing bonus of a revelation stage (+4 / +9 / +20).
int largePerkExtraDamageHealing(int stage);

int basicModCount();
int supremeModCount();
/// The basic mod exists (mod 32 is an empty slot of the table).
bool hasBasicMod(int mod);
/// CIP : the supreme mod is a spell augmentation.
bool isAugmentationMod(int mod);
/// CIP mod list (0x101122xxx): supreme mods 0..3 and 5 suit every vocation, the rest one vocation index.
bool supremeModForVocation(int mod, int vocationIndex);
/// CIP : the grade points a vocation can earn (23 supreme + the basic mods).
int totalModGradePoints(int vocationIndex);
/// CIP : the mods at the highest grade, one promotion point each.
int modGradePoints(const TGrades& basicGrades, const TGrades& supremeGrades);

/// Gem-quality label helper (Winter External dropped Vessel Resonance activation ids).
QString vesselActivation(int resonance, bool plural);
/// Lesser/regular/greater gem quality ↔ vessel slot capacity (1/2/3 mods).
bool vesselMatchesGem(int quality, int resonance);
/// Gem Vessel perk description + gem quality info (replaces removed resonance tooltip).
QString vesselBonusInformation();
/// CipSoft `skill_wheel_dialog_gem_information`.
QString gemInformation();

/// The unrevealed gem object of a vocation index and quality (Crystal vocations.xml).
int unrevealedGemTypeId(int vocationIndex, int quality);

/// The mods of a socketed gem in an export code (CIP); -1 none.
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
    std::array<TCodeGem, VesselCount> gems{};
};

/// CIP : `<K|P|S|D|M>0` and the url base64 (no padding) of the raw
/// deflated 36 slice points and 4 vessel gems.
QString encodeWheelCode(const TWheel& wheel, const std::vector<TGem>& gems, int vocationIndex);
/// CIP : that format, or the legacy `EK|RP|MS|ED|EM` + 48 base64
/// characters of the slice points; empty on any decoding error.
std::optional<TWheelCode> decodeWheelCode(const QString& code);
/// CIP : the slices respect their maximum and paths.
bool isValidWheelCode(const TWheelCode& code);

}
}
