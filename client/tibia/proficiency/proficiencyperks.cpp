#include "proficiency/proficiencyperks.h"

#include "proficiency/proficiencycatalogue.h"
#include "spells/spellinfostorage.h"

#include <QLocale>
#include <QRegularExpression>

#include <array>
#include <cmath>

namespace tibia::proficiency {

namespace {

constexpr int AugmentPerk = 5;

/// CIP FUN_100ee6830's qtTrId ids, by perk type.
constexpr std::array<const char*, 36> NameIds {
    "weapon_proficiency_attack_perk_name",
    "weapon_proficiency_defence_perk_name",
    "weapon_proficiency_weapon_shield_modifier_perk_name",
    "weapon_proficiency_skill_bonus_perk_name",
    "weapon_proficiency_magic_boost_perk_name",
    "weapon_proficiency_augment_perk_name",
    "weapon_proficiency_bestiary_damage_perk_name",
    "weapon_proficiency_powerful_foe_bonus_perk_name",
    "weapon_proficiency_crit_chance_perk_name",
    "weapon_proficiency_element_crit_chance_perk_name",
    "weapon_proficiency_rune_crit_chance_perk_name",
    "weapon_proficiency_autoattacks_crit_chance_perk_name",
    "weapon_proficiency_crit_damage_perk_name",
    "weapon_proficiency_element_crit_damage_perk_name",
    "weapon_proficiency_rune_crit_damage_perk_name",
    "weapon_proficiency_autoattacks_crit_damage_perk_name",
    "weapon_proficiency_mana_leech_perk_name",
    "weapon_proficiency_life_leech_perk_name",
    "weapon_proficiency_mana_on_hit_perk_name",
    "weapon_proficiency_life_on_hit_perk_name",
    "weapon_proficiency_mana_on_kill_perk_name",
    "weapon_proficiency_life_on_kill_perk_name",
    "weapon_proficiency_perfect_shot_damage_perk_name",
    "weapon_proficiency_ranged_hit_chance_perk_name",
    "weapon_proficiency_attack_range_perk_name",
    "weapon_proficiency_skill_percentage_autoattacks_perk_name",
    "weapon_proficiency_skill_percentage_spells_perk_name",
    "weapon_proficiency_skill_percentage_healing_perk_name",
    "weapon_proficiency_alpha_strike_extra_damage_perk_name",
    "weapon_proficiency_omega_strike_extra_damage_perk_name",
    "weapon_proficiency_armor_penetration_perk_name",
    "weapon_proficiency_elemental_pierce_perk_name",
    "weapon_proficiency_homing_missile_perk_name",
    "weapon_proficiency_highest_combat_skill_percentage_autoattacks_perk_name",
    "weapon_proficiency_highest_combat_skill_percentage_spells_perk_name",
    "weapon_proficiency_highest_combat_skill_percentage_healing_perk_name",
};

/// CIP DAT_1022b6ba8: the "proficiency-icons" tile of perk type 1..30.
constexpr std::array<int, 30> PerkIcons { 1, 2, 0, 0, 0, 0, 3, 4, 0, 5, 6, 7, 0, 8, 9, 11, 10, 13, 12, 15, 14, 16, 17, 18, 0, 0, 0, 19, 20, 21 };
/// CIP DAT_1022b6b40: the skill strips' tile of skill id 1..13.
constexpr std::array<int, 13> SkillIcons { 4, -1, -1, -1, -1, 5, 6, 0, 2, 1, 3, -1, 7 };
/// CIP DAT_1022b6a90: the "icons-skillwheel-smallperks" tile of augment type 0..21.
constexpr std::array<int, 22> AugmentIcons { 9, 8, 6, 6, 5, 10, 5, 5, 9, 9, 9, 9, 9, 9, 11, 12, 7, 7, 6, 9, 5, 6 };
/// CIP DAT_1022f52d8 + FUN_100a98d30: the skill name of skill id 1..13.
constexpr std::array<const char*, 13> SkillNameIds {
    "charinfo_skill_magic",   "charinfo_source_skill",   "charinfo_source_skill", "charinfo_source_skill",
    "charinfo_source_skill",  "charinfo_skill_shielding", "charinfo_skill_distance", "charinfo_skill_sword",
    "charinfo_skill_club",    "charinfo_skill_axe",       "charinfo_skill_fist",     "charinfo_source_skill",
    "charinfo_skill_fishing",
};
/// CIP FUN_100ee6b70: the augment type names (DAT_1022f5280).
constexpr std::array<const char*, 22> AugmentTypeIds {
    "dummy_unknown",
    "charinfo_augmenttype_manacost",
    "charinfo_augmenttype_damage",
    "healing",
    "charinfo_augmenttype_duration",
    "charinfo_augmenttype_chainlength",
    "cooldown",
    "charinfo_augmenttype_secondarygroupcooldown",
    "charinfo_augmenttype_angle",
    "charinfo_augmenttype_sapstrength",
    "charinfo_augmenttype_range",
    "augmentation_description_affected_area",
    "charinfo_augmenttype_boostedeffect",
    "charinfo_augmenttype_skillincrease",
    "life_leech",
    "mana_leech",
    "skill_wheel_critical_extra_damage",
    "charinfo_augmenttype_criticalchance",
    "dummy_unknown",
    "charinfo_augmenttype_shape",
    "charinfo_augmenttype_delay",
    "charinfo_augmenttype_nextautoattackreduction",
};

/// CIP FUN_10095c2f0: English, fixed point, no decimals for zero.
QString fixed(const double value, const int decimals)
{
    return QLocale(QLocale::English).toString(value, 'f', value == 0.0 ? 0 : decimals);
}

/// CIP FUN_10095c380(value, 0, 2): two decimals without trailing zeros.
QString trimmed(const double value)
{
    QString text = QLocale(QLocale::English).toString(value, 'f', 2);
    if (text.contains(QLatin1Char('.'))) {
        static const QRegularExpression zeros(QStringLiteral("0+$"));
        static const QRegularExpression dot(QStringLiteral("\\.$"));
        text.remove(zeros);
        text.remove(dot);
    }
    return text;
}

QString percent(const TProficiencyPerk& perk, const int decimals = 2) { return fixed(perk.value * 100.0, decimals); }

QString skillName(const int skillId)
{
    return qtTrId(skillId >= 1 && skillId <= 13 ? SkillNameIds[skillId - 1] : "charinfo_source_skill");
}

/// CIP FUN_100a45e80 after the element-to-damage-type switch of the element perks.
QString elementName(const int element)
{
    switch (element) {
    case 0x8: return qtTrId("damagetype_fire");
    case 0x10: return qtTrId("damagetype_earth");
    case 0x20: return qtTrId("damagetype_energy");
    case 0x40: return qtTrId("damagetype_ice");
    case 0x80: return qtTrId("damagetype_holy");
    case 0x100: return qtTrId("damagetype_death");
    case 0x100000: return qtTrId("damagetype_healing");
    default: return qtTrId("damagetype_physical");
    }
}

/// The element strips' tile (CIP FUN_100929630 and its copies).
int elementIcon(const int element)
{
    switch (element) {
    case 0x1:
    case 0x100000: return 0;
    case 0x8: return 1;
    case 0x10: return 3;
    case 0x20: return 2;
    case 0x40: return 4;
    case 0x80: return 6;
    case 0x100: return 5;
    default: return -1;
    }
}

/// CIP FUN_100928e50.
int magicLevelIcon(const int element)
{
    switch (element) {
    case 0x1:
    case 0x8: return 0;
    case 0x10: return 2;
    case 0x20: return 3;
    case 0x40: return 1;
    case 0x80: return 5;
    case 0x100: return 4;
    case 0x100000: return 6;
    default: return -1;
    }
}

int skillIcon(const int skillId) { return skillId >= 1 && skillId <= 13 ? SkillIcons[skillId - 1] : -1; }

QUrl strip(const char* name, const int tile) { return QUrl(QStringLiteral("image://%1/%2").arg(QLatin1String(name)).arg(tile)); }

const spells::TSpellInfo* augmentSpell(const TProficiencyPerk& perk)
{
    const spells::TSpellInfoStorage* storage = spells::TSpellInfoStorage::instance();
    return storage ? storage->spell(perk.spellId) : nullptr;
}

/// CIP FUN_100ee6b70's value text: signed, a percentage unless the augment
/// type counts plainly, "s" for durations, nothing for the flag types.
QString augmentValue(const TProficiencyPerk& perk)
{
    const int type = perk.augmentType;
    const auto in = [type](const uint32_t mask) { return type >= 0 && type < 21 && ((mask >> type) & 1) != 0; };
    if (in(0x81910u))
        return {};
    QString text = (perk.value < 0.0 || type == 21) ? QString() : QStringLiteral("+");
    if (type == 20)
        text += trimmed(std::fabs(perk.value));
    else if (in(0x1004e2u))
        text += trimmed(perk.value);
    else
        text += trimmed(perk.value * 100.0 * (type == 21 ? -1.0 : 1.0)) + QLatin1Char('%');
    if (in(0x1000c0u))
        text += QLatin1Char('s');
    return text;
}

QString augmentDescription(const TProficiencyPerk& perk)
{
    const char* id = nullptr;
    switch (perk.augmentType) {
    case 2: id = "weapon_proficiency_spell_augment_percent_damage_description"; break;
    case 3: id = "weapon_proficiency_spell_augment_healing_description"; break;
    case 5: id = "weapon_proficiency_spell_augment_chain_length_description"; break;
    case 6: id = "weapon_proficiency_spell_augment_cooldown_reduction_description"; break;
    case 14: id = "weapon_proficiency_spell_augment_life_leech_description"; break;
    case 15: id = "weapon_proficiency_spell_augment_mana_leech_description"; break;
    case 16: id = "weapon_proficiency_spell_augment_critical_damage_description"; break;
    case 17: id = "weapon_proficiency_spell_augment_critical_chance_description"; break;
    default: return qtTrId("dummy_unknown");
    }
    const spells::TSpellInfo* spell = augmentSpell(perk);
    return qtTrId(id).arg(augmentValue(perk), spell ? spell->name : QString());
}

}

QString perkName(const TProficiencyPerk& perk)
{
    if (perk.type < 0 || perk.type >= static_cast<int>(NameIds.size()))
        return qtTrId("weapon_proficiency_unknown_perk_name");
    if (perk.type == AugmentPerk) {
        const spells::TSpellInfo* spell = augmentSpell(perk);
        return qtTrId(spell && spell->runeObjectId != 0 ? "weapon_proficiency_augment_perk_name_rune"
                                                         : "weapon_proficiency_augment_perk_name");
    }
    return qtTrId(NameIds[perk.type]);
}

QString perkDescription(const TProficiencyPerk& perk)
{
    const qlonglong integer = static_cast<qlonglong>(perk.value);
    switch (perk.type) {
    case 0: return qtTrId("weapon_proficiency_attack_damage_perk_description").arg(integer);
    case 1: return qtTrId("weapon_proficiency_defence_perk_description").arg(integer);
    case 2: return qtTrId("weapon_proficiency_weapon_shield_perk_description").arg(integer);
    case 3: return qtTrId("weapon_proficiency_skill_perk_description").arg(integer).arg(skillName(perk.skillId));
    case 4: return qtTrId("weapon_proficiency_specialized_magic_level_perk_description").arg(integer).arg(elementName(perk.element));
    case 5: return augmentDescription(perk);
    case 6:
        return qtTrId("weapon_proficiency_damage_bestiary_perk_description")
            .arg(percent(perk), perk.bestiaryName.isEmpty() ? QStringLiteral("Unknown") : perk.bestiaryName);
    case 7: return qtTrId("weapon_proficiency_damage_special_monsters_perk_description").arg(percent(perk));
    case 8: return qtTrId("weapon_proficiency_critical_hit_chance_perk_description").arg(percent(perk));
    case 9: return qtTrId("weapon_proficiency_critical_hit_chance_element_perk_description").arg(percent(perk), elementName(perk.element));
    case 10: return qtTrId("weapon_proficiency_critical_hit_chance_rune_perk_description").arg(percent(perk));
    case 11: return qtTrId("weapon_proficiency_critical_hit_chance_autoattack_perk_description").arg(percent(perk));
    case 12: return qtTrId("weapon_proficiency_critical_extra_damage_perk_description").arg(percent(perk));
    case 13: return qtTrId("weapon_proficiency_critical_extra_damage_element_perk_description").arg(percent(perk), elementName(perk.element));
    case 14: return qtTrId("weapon_proficiency_critical_extra_damage_rune_perk_description").arg(percent(perk));
    case 15: return qtTrId("weapon_proficiency_critical_extra_damage_autoattack_perk_description").arg(percent(perk));
    case 16: return qtTrId("weapon_proficiency_mana_leech_percentage_perk_description").arg(percent(perk));
    case 17: return qtTrId("weapon_proficiency_life_leech_percentage_perk_description").arg(percent(perk));
    case 18: return qtTrId("weapon_proficiency_mana_on_hit_perk_description").arg(integer);
    case 19: return qtTrId("weapon_proficiency_life_on_hit_perk_description").arg(integer);
    case 20: return qtTrId("weapon_proficiency_mana_on_kill_perk_description").arg(integer);
    case 21: return qtTrId("weapon_proficiency_life_on_kill_perk_description").arg(integer);
    case 22: return qtTrId("weapon_proficiency_damage_at_range_perk_description").arg(integer).arg(perk.range);
    case 23: return qtTrId("weapon_proficiency_ranged_chance_to_hit_perk_description").arg(percent(perk));
    case 24: return qtTrId("weapon_proficiency_attack_range_perk_description").arg(integer);
    case 25:
        return qtTrId("weapon_proficiency_skilllevel_percentage_autoattack_perk_description").arg(percent(perk), skillName(perk.skillId));
    case 26:
        return qtTrId("weapon_proficiency_skilllevel_percentage_spelldamage_perk_description").arg(percent(perk), skillName(perk.skillId));
    case 27:
        return qtTrId("weapon_proficiency_skilllevel_percentage_healing_perk_description").arg(percent(perk), skillName(perk.skillId));
    case 28: return qtTrId("weapon_proficiency_alpha_strike_extra_damage_perk_description").arg(percent(perk));
    case 29: return qtTrId("weapon_proficiency_omega_strike_extra_damage_perk_description").arg(percent(perk));
    case 30: return qtTrId("weapon_proficiency_armor_penetration_perk_description").arg(percent(perk));
    case 31: return qtTrId("weapon_proficiency_elemental_pierce_perk_description").arg(percent(perk, 0), elementName(perk.element));
    case 32:
        return qtTrId("weapon_proficiency_homing_missile_perk_description")
            .arg(fixed(perk.probability * 100.0, 0), elementName(perk.element), fixed(perk.multiplier * 100.0, 0));
    case 33:
        return qtTrId("weapon_proficiency_highest_combat_skilllevel_percentage_autoattack_perk_description").arg(percent(perk));
    case 34:
        return qtTrId("weapon_proficiency_highest_combat_skilllevel_percentage_spelldamage_perk_description").arg(percent(perk));
    case 35:
        return qtTrId("weapon_proficiency_highest_combat_skilllevel_percentage_healing_perk_description").arg(percent(perk));
    default: return {};
    }
}

QUrl perkImageSource(const TProficiencyPerk& perk)
{
    switch (perk.type) {
    case 3: return strip("proficiency-icons-skills", skillIcon(perk.skillId));
    case 4: return strip("proficiency-icons-specialized-magic-level", magicLevelIcon(perk.element));
    case 5: {
        const spells::TSpellInfo* spell = augmentSpell(perk);
        return QUrl(spell ? spells::TSpellInfoStorage::iconUrl(*spell) : QStringLiteral("image://spell-icons-32x32/0"));
    }
    case 6: return strip("proficiency-icons-monsterclasses", perk.bestiaryId - 1);
    case 9: return strip("proficiency-icons-critchance-elements", elementIcon(perk.element));
    case 13: return strip("proficiency-icons-critdamage-elements", elementIcon(perk.element));
    case 25: return strip("proficiency-icons-skilllevel-autoattack", skillIcon(perk.skillId));
    case 26: return strip("proficiency-icons-skilllevel-spells", skillIcon(perk.skillId));
    case 27: return strip("proficiency-icons-skilllevel-healing", skillIcon(perk.skillId));
    case 31: return strip("proficiency-icons-elemental-pierce", elementIcon(perk.element));
    case 32: return strip("proficiency-icons-homing-missile", elementIcon(perk.element));
    case 33: return strip("proficiency-icons-skilllevel-autoattack", 8);
    case 34: return strip("proficiency-icons-skilllevel-spells", 8);
    case 35: return strip("proficiency-icons-skilllevel-healing", 8);
    default:
        return strip("proficiency-icons", perk.type >= 1 && perk.type <= 30 ? PerkIcons[perk.type - 1] : 0);
    }
}

bool perkHasAugmentEffect(const TProficiencyPerk& perk) { return perk.type == AugmentPerk; }

QUrl perkAugmentEffectImageSource(const TProficiencyPerk& perk)
{
    if (perk.type != AugmentPerk)
        return {};
    const int tile = perk.augmentType >= 0 && perk.augmentType < static_cast<int>(AugmentIcons.size()) ? AugmentIcons[perk.augmentType] : 0;
    return strip("icons-skillwheel-smallperks", tile);
}

QString perkAugmentSpellName(const TProficiencyPerk& perk)
{
    const spells::TSpellInfo* spell = augmentSpell(perk);
    return spell ? spell->name : QString();
}

QString perkAugmentTypeName(const TProficiencyPerk& perk)
{
    const bool known = perk.augmentType >= 0 && perk.augmentType < static_cast<int>(AugmentTypeIds.size());
    return qtTrId(known ? AugmentTypeIds[perk.augmentType] : "dummy_unknown");
}

TQMLProficiencyPerk::TQMLProficiencyPerk(const int index, const TProficiencyPerk& perk, const bool picked, const bool shaped,
                                         const uint rank, QObject* parent)
    : cip::tibia::gamewindow::TQMLProficiencyPerk(parent)
{
    m_perkIndex = index;
    m_perkDescription = proficiency::perkDescription(perk);
    m_perkName = proficiency::perkName(perk);
    m_imageSource = perkImageSource(perk);
    m_hasAugmentEffect = perkHasAugmentEffect(perk);
    m_augmentEffectImageSource = perkAugmentEffectImageSource(perk);
    m_isPicked = picked;
    m_isShaped = shaped;
    m_perkRank = shaped ? rank : 0;
}

TQMLProficiencyLevelInformation::TQMLProficiencyLevelInformation(const int levelIndex, const int tableLevels, const uint16_t typeId,
                                                                 const TProficiencyLevel& level, const TPerkPick* localPick,
                                                                 const std::vector<TShapedSlot>& shaped, const bool serverPicked,
                                                                 QObject* parent)
    : cip::tibia::gamewindow::TQMLProficiencyLevelInformation(parent)
{
    m_levelIndex = levelIndex;
    m_neededXP = static_cast<int>(TProficiencyStorage::levelXP(levelIndex + 1, tableLevels, typeId));
    m_hasServerSelectedPerk = serverPicked;
    for (size_t index = 0; index < level.perks.size(); ++index) {
        const auto slot = std::ranges::find_if(shaped, [&](const TShapedSlot& entry) {
            return entry.level == levelIndex && entry.perk == index;
        });
        const bool isShaped = slot != shaped.end();
        const TProficiencyPerk perk = isShaped ? shapedPerk(slot->perkType, slot->rank) : level.perks[index];
        const bool picked = localPick && localPick->picked && localPick->perk == index;
        if (picked) {
            m_selectedPerkDescription = perkDescription(perk);
            m_isPerkSelected = true;
        }
        m_perks << new TQMLProficiencyPerk(static_cast<int>(index), perk, picked, isShaped, isShaped ? slot->rank : 0, this);
    }
}

}
