#include "protocol/playerwire.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

bool readSkill(ClassicReader& r, ProtocolSkill* out)
{
    uint16_t level = 0, base = 0, loyalty = 0, percent = 0;
    if (!r.u16(level) || !r.u16(base) || !r.u16(loyalty) || !r.u16(percent))
        return false;
    out->set_level(level);
    out->set_base(base);
    out->set_loyalty(loyalty);
    out->set_percent_hundredths(percent);
    return true;
}

bool readSkills(ClassicReader& r, GameserverMessagePlayerSkills* m)
{
    // Magic level, fist, club, sword, axe, distance, shielding, fishing.
    for (int i = 0; i < 8; ++i) {
        if (!readSkill(r, m->add_skills()))
            return false;
    }
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t type = 0;
        int16_t value = 0;
        if (!r.u8(type) || !r.s16(value))
            return false;
        ProtocolSkillModifier* modifier = m->add_modifiers();
        modifier->set_type(type);
        modifier->set_value(value);
    }
    uint32_t capacity = 0, baseCapacity = 0;
    uint16_t flat = 0, attack = 0;
    uint8_t attackElement = 0, convertedElement = 0;
    double converted = 0;
    if (!r.u32(capacity) || !r.u32(baseCapacity) || !r.u16(flat) || !r.u16(attack) || !r.u8(attackElement)
        || !r.real(converted) || !r.u8(convertedElement))
        return false;
    m->set_capacity(capacity);
    m->set_base_capacity(baseCapacity);
    m->set_flat_damage_healing(flat);
    m->set_attack(attack);
    m->set_attack_element(attackElement);
    m->set_converted_damage(converted);
    m->set_converted_element(convertedElement);
    double lifeLeech = 0, manaLeech = 0, criticalChance = 0, criticalDamage = 0, onslaught = 0;
    if (!r.real(lifeLeech) || !r.real(manaLeech) || !r.real(criticalChance) || !r.real(criticalDamage) || !r.real(onslaught))
        return false;
    m->set_life_leech(lifeLeech);
    m->set_mana_leech(manaLeech);
    m->set_critical_chance(criticalChance);
    m->set_critical_damage(criticalDamage);
    m->set_onslaught(onslaught);
    uint16_t defense = 0, armor = 0, mantra = 0, reflection = 0;
    double mitigation = 0, dodge = 0;
    if (!r.u16(defense) || !r.u16(armor) || !r.u16(mantra) || !r.real(mitigation) || !r.real(dodge) || !r.u16(reflection))
        return false;
    m->set_defense(defense);
    m->set_armor(armor);
    m->set_mantra(mantra);
    m->set_mitigation(mitigation);
    m->set_dodge(dodge);
    m->set_damage_reflection(reflection);
    uint8_t absorbs = 0;
    if (!r.u8(absorbs))
        return false;
    for (uint8_t i = 0; i < absorbs; ++i) {
        uint8_t combat = 0;
        double value = 0;
        if (!r.u8(combat) || !r.real(value))
            return false;
        ProtocolCombatAbsorb* absorb = m->add_absorbs();
        absorb->set_combat(combat);
        absorb->set_value(value);
    }
    double momentum = 0, transcendence = 0, amplification = 0;
    if (!r.real(momentum) || !r.real(transcendence) || !r.real(amplification))
        return false;
    m->set_momentum(momentum);
    m->set_transcendence(transcendence);
    m->set_amplification(amplification);
    return true;
}

}

bool isPlayerMessage(const int opcode)
{
    switch (opcode) {
    case 159: // PlayerDataBasic
    case 160: // PlayerDataCurrent
    case 161: // PlayerSkills
    case 162: // PlayerState
    case 163: // UpdateTarget
    case 164: // SpellDelay
    case 165: // SpellGroupDelay
    case 166: // MultiUseDelay
    case 167: // SetTactics
        return true;
    default:
        return false;
    }
}

size_t fillPlayerMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 159: {
        auto* m = msg.MutableExtension(GameserverMessagePlayerDataBasic::gameserver_message_player_data_basic);
        uint8_t premium = 0, vocation = 0, reachedMain = 0, magicShield = 0;
        uint32_t premiumUntil = 0;
        uint16_t count = 0;
        if (!r.u8(premium) || !r.u32(premiumUntil) || !r.u8(vocation) || !r.u8(reachedMain) || !r.u16(count))
            return 0;
        m->set_premium(premium != 0);
        m->set_premium_until(premiumUntil);
        m->set_vocation(vocation % 10);
        m->set_promoted(vocation > 9);
        m->set_reached_main(reachedMain != 0);
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t spell = 0;
            if (!r.u16(spell))
                return 0;
            m->add_spells(spell);
        }
        if (!r.u8(magicShield))
            return 0;
        m->set_magic_shield(magicShield != 0);
        return r.i;
    }
    case 160: {
        auto* m = msg.MutableExtension(GameserverMessagePlayerDataCurrent::gameserver_message_player_data_current);
        uint32_t health = 0, maxHealth = 0, freeCapacity = 0, mana = 0, maxMana = 0, manaShield = 0, maxManaShield = 0;
        uint64_t experience = 0;
        uint16_t level = 0, levelPercent = 0, baseXp = 0, lowLevel = 0, storeBoost = 0, staminaFactor = 0;
        uint16_t stamina = 0, baseSpeed = 0, regeneration = 0, training = 0, storeBoostTime = 0;
        uint16_t dailyRewardBoost = 0, dailyRewardBoostTime = 0;
        uint8_t soul = 0, purchasable = 0;
        if (!r.u32(health) || !r.u32(maxHealth) || !r.u32(freeCapacity) || !r.u64(experience) || !r.u16(level)
            || !r.u16(levelPercent) || !r.u16(baseXp) || !r.u16(lowLevel) || !r.u16(storeBoost) || !r.u16(staminaFactor)
            || !r.u32(mana) || !r.u32(maxMana) || !r.u8(soul) || !r.u16(stamina) || !r.u16(baseSpeed)
            || !r.u16(regeneration) || !r.u16(training) || !r.u16(storeBoostTime) || !r.u8(purchasable)
            || !r.u32(manaShield) || !r.u32(maxManaShield) || !r.u16(dailyRewardBoost) || !r.u16(dailyRewardBoostTime))
            return 0;
        m->set_health(health);
        m->set_max_health(maxHealth);
        m->set_free_capacity(freeCapacity);
        m->set_experience(experience);
        m->set_level(level);
        m->set_level_percent_hundredths(levelPercent);
        m->set_base_xp_gain(baseXp);
        m->set_low_level_xp_bonus(lowLevel);
        m->set_store_xp_boost(storeBoost);
        m->set_stamina_xp_factor(staminaFactor);
        m->set_mana(mana);
        m->set_max_mana(maxMana);
        m->set_soul(soul);
        m->set_stamina_minutes(stamina);
        m->set_base_speed(baseSpeed);
        m->set_regeneration_seconds(regeneration);
        m->set_offline_training_minutes(training);
        m->set_store_xp_boost_seconds(storeBoostTime);
        m->set_store_xp_boost_purchasable(purchasable != 0);
        m->set_mana_shield(manaShield);
        m->set_max_mana_shield(maxManaShield);
        m->set_daily_reward_xp_boost(dailyRewardBoost);
        m->set_daily_reward_xp_boost_seconds(dailyRewardBoostTime);
        return r.i;
    }
    case 161:
        if (!readSkills(r, msg.MutableExtension(GameserverMessagePlayerSkills::gameserver_message_player_skills)))
            return 0;
        return r.i;
    case 162: {
        auto* m = msg.MutableExtension(GameserverMessagePlayerState::gameserver_message_player_state);
        uint64_t states = 0;
        uint8_t counter = 0;
        if (!r.u64(states) || !r.u8(counter))
            return 0;
        m->set_states(states);
        m->set_counter(counter);
        return r.i;
    }
    case 163: {
        auto* m = msg.MutableExtension(GameserverMessageUpdateTarget::gameserver_message_update_target);
        uint32_t sequence = 0, target = 0;
        if (!r.u32(sequence) || !r.u32(target))
            return 0;
        m->set_sequence(sequence);
        m->set_target(target);
        return r.i;
    }
    case 164: {
        auto* m = msg.MutableExtension(GameserverMessageSpellDelay::gameserver_message_spell_delay);
        uint16_t spell = 0;
        uint32_t delay = 0;
        if (!r.u16(spell) || !r.u32(delay))
            return 0;
        m->set_spell_id(spell);
        m->set_delay(delay);
        return r.i;
    }
    case 165: {
        auto* m = msg.MutableExtension(GameserverMessageSpellGroupDelay::gameserver_message_spell_group_delay);
        uint8_t group = 0;
        uint32_t delay = 0;
        if (!r.u8(group) || !r.u32(delay))
            return 0;
        m->set_group_id(group);
        m->set_delay(delay);
        return r.i;
    }
    case 166: {
        auto* m = msg.MutableExtension(GameserverMessageMultiUseDelay::gameserver_message_multi_use_delay);
        uint32_t delay = 0;
        if (!r.u32(delay))
            return 0;
        m->set_delay(delay);
        return r.i;
    }
    case 167: {
        auto* m = msg.MutableExtension(GameserverMessageSetTactics::gameserver_message_set_tactics);
        uint8_t chase = 0, secure = 0, pvp = 0;
        if (!r.u8(chase) || !r.u8(secure) || !r.u8(pvp))
            return 0;
        m->set_chase_opponents(chase != 0);
        m->set_secure_mode(secure != 0);
        m->set_pvp_mode(pvp);
        return r.i;
    }
    default:
        return 0;
    }
}

}
