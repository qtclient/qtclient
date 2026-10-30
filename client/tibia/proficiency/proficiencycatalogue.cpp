#include "proficiency/proficiencycatalogue.h"

#include <array>

namespace tibia::proficiency {

namespace {

constexpr int AugmentPerk = 5;
constexpr int BestiaryPerk = 6;

/// CIP FUN_101559950's spell augment block: each profession region holds five
/// rows of ten indices (augment types 0x11, 0x10, 0x2, 0xF, 0xE), columns 1..6
/// are the region's spells.
constexpr int RegionSize = 50;
constexpr int RegionCount = 5;
constexpr std::array<int, 5> AugmentRows { 0x11, 0x10, 0x2, 0xF, 0xE };
constexpr std::array<std::array<int, 6>, RegionCount> RegionSpells { {
    { 0x50, 0x69, 0x6a, 0x3b, 0x13c, 0x105 },
    { 0x7c, 0x12e, 0x12f, 0x102, 0x39, 0x7a },
    { 0xd, 0x18, 0xf0, 0x104, 0x136, 0x17 },
    { 0x2b, 0x78, 0x107, 0x106, 0x13d, 0x13e },
    { 0x121, 0x120, 0x126, 0x11f, 0x12d, 0x122 },
} };

constexpr int FirstBestiaryIndex = 251;
constexpr std::array<const char*, 21> BestiaryClasses {
    "Amphibic", "Aquatic",  "Bird",        "Construct", "Demon",   "Dragon", "Elemental",
    "Fey",      "Giant",    "Human",       "Humanoid",  "Lycanthrope", "Magical", "Mammal",
    "Plant",    "Reptile",  "Slime",       "Undead",    "Vermin",  "Extra Dimensional", "Inkborn",
};

struct TSingle
{
    int index;
    int type;
};
constexpr std::array<TSingle, 13> SinglePerks { {
    { 281, 10 }, { 282, 11 }, { 283, 14 }, { 284, 15 }, { 285, 19 }, { 286, 20 }, { 287, 21 },
    { 291, 33 }, { 292, 34 }, { 293, 35 },
    { 321, 28 }, { 322, 29 }, { 323, 30 },
} };

/// CIP DAT_1023de7fc / 0x1023de9fc / 0x1023debfc / 0x1023dedfc / 0x1023deffc:
/// every profession's list starts with the shared block.
void appendCommon(std::vector<int>& options)
{
    for (int index = 251; index <= 271; ++index)
        options.push_back(index);
    for (int index = 281; index <= 287; ++index)
        options.push_back(index);
    for (int index = 291; index <= 293; ++index)
        options.push_back(index);
    for (int index = 321; index <= 323; ++index)
        options.push_back(index);
}

}

std::optional<TProficiencyPerk> shapePerk(const int index)
{
    if (index >= 1 && index < RegionSize * RegionCount) {
        const int region = index / RegionSize;
        const int offset = index % RegionSize;
        const int row = offset / 10;
        const int column = offset % 10;
        if (row >= static_cast<int>(AugmentRows.size()) || column < 1 || column > 6)
            return std::nullopt;
        TProficiencyPerk perk;
        perk.type = AugmentPerk;
        perk.augmentType = AugmentRows[row];
        perk.spellId = RegionSpells[region][column - 1];
        return perk;
    }
    if (index >= FirstBestiaryIndex && index < FirstBestiaryIndex + static_cast<int>(BestiaryClasses.size())) {
        TProficiencyPerk perk;
        perk.type = BestiaryPerk;
        perk.bestiaryId = index - FirstBestiaryIndex + 1;
        perk.bestiaryName = QString::fromLatin1(BestiaryClasses[index - FirstBestiaryIndex]);
        return perk;
    }
    for (const TSingle& single : SinglePerks) {
        if (single.index == index) {
            TProficiencyPerk perk;
            perk.type = single.type;
            return perk;
        }
    }
    return std::nullopt;
}

TProficiencyPerk perkAtRank(TProficiencyPerk perk, const int rank)
{
    const double r = rank;
    switch (perk.type) {
    case AugmentPerk:
        switch (perk.augmentType) {
        case 0x2:
        case 0x11: perk.value = 0.002 * r + 0.01; break;
        case 0xE: perk.value = 0.011 * r + 0.01; break;
        case 0xF: perk.value = 0.005 * r + 0.01; break;
        case 0x10: perk.value = 0.015 * r + 0.05; break;
        default: perk.value = 0.0; break;
        }
        break;
    case BestiaryPerk: perk.value = 0.002 * r + 0.005; break;
    case 10: perk.value = 0.001 * r + 0.005; break;
    case 11: perk.value = 0.002 * r + 0.005; break;
    case 14: perk.value = 0.013 * r + 0.02; break;
    case 15: perk.value = 0.017 * r + 0.03; break;
    case 19: perk.value = rank + 2; break;
    case 20: perk.value = 2 * rank + 4; break;
    case 21: perk.value = 4 * rank + 10; break;
    case 28: perk.value = 0.008 * r + 0.02; break;
    case 29: perk.value = 0.002 * r + 0.005; break;
    case 30: perk.value = 0.006 * r + 0.04; break;
    case 33: perk.value = 0.008 * r + 0.02; break;
    case 34: perk.value = 0.007 * r + 0.01; break;
    case 35: perk.value = 0.008 * r + 0.02; break;
    default: break;
    }
    return perk;
}

std::vector<int> shapeOptions(const int profession)
{
    if (profession < 1 || profession > RegionCount)
        return {};
    std::vector<int> options;
    appendCommon(options);
    const int base = (profession - 1) * RegionSize;
    for (int row = 0; row < static_cast<int>(AugmentRows.size()); ++row) {
        for (int column = 1; column <= 6; ++column)
            options.push_back(base + row * 10 + column);
    }
    return options;
}

TProficiencyPerk shapedPerk(const int index, const int rank)
{
    const std::optional<TProficiencyPerk> perk = shapePerk(index);
    return perkAtRank(perk.value_or(TProficiencyPerk {}), rank);
}

}
