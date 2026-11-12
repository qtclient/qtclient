#include "game/playerdata.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/localplayersessionthing.h>
#include <tibia/game/tgamenotify.h>

#include <string_view>

namespace tibia::game {
namespace {

TPlayerData* g_livePlayerData = nullptr;

} // namespace

TPlayerData::TPlayerData(QObject* parent)
    : cip::tibia::game::TPlayerData(parent)
{
}

TPlayerData* TPlayerData::liveInstance()
{
    return g_livePlayerData;
}

void TPlayerData::setLiveInstance(TPlayerData* data)
{
    g_livePlayerData = data;
}

// b1/25629e06a39166a2
void TPlayerData::name()
{
}

void TPlayerData::v71(const qulonglong value)
{
    setResourceBalance(tibia::client::RESOURCE_BANK_BALANCE, static_cast<uint64_t>(value));
}

qulonglong TPlayerData::v72() const
{
    return static_cast<qulonglong>(resourceBalance(tibia::client::RESOURCE_BANK_BALANCE));
}

void TPlayerData::v73(const qulonglong value)
{
    setResourceBalance(tibia::client::RESOURCE_GOLD_EQUIPPED, static_cast<uint64_t>(value));
}

qulonglong TPlayerData::v75() const
{
    return static_cast<qulonglong>(totalMoney());
}

void TPlayerData::v76(const qulonglong value)
{
    setResourceBalance(tibia::client::RESOURCE_GUILD_BANK_BALANCE, static_cast<uint64_t>(value));
}

qulonglong TPlayerData::v77() const
{
    return static_cast<qulonglong>(resourceBalance(tibia::client::RESOURCE_GUILD_BANK_BALANCE));
}

bool TPlayerData::v78() const
{
    return hasResourceBalance(tibia::client::RESOURCE_GUILD_BANK_BALANCE);
}

void TPlayerData::v91(const long value)
{
    setLevel(static_cast<uint16_t>(value), m_levelPercent);
}

long TPlayerData::v92() const
{
    return static_cast<long>(m_level);
}

void TPlayerData::v93(const long value)
{
    setLevel(m_level, static_cast<uint16_t>(value));
}

long TPlayerData::v94() const
{
    return static_cast<long>(m_levelPercent);
}

void TPlayerData::v95(const long value)
{
    setVocation(static_cast<uint8_t>(value));
}

long TPlayerData::v96() const
{
    return static_cast<long>(m_vocation);
}

uint16_t TPlayerData::levelPercent() const
{
    return tibia::game::cipFeature(tibia::client::GameLevelPercentU16) ? m_levelPercent / 100 : m_levelPercent;
}

uint64_t TPlayerData::resourceBalance(const tibia::client::ResourceTypes_t type) const
{
    const auto it = m_resourcesBalance.find(type);
    return it != m_resourcesBalance.end() ? it->second : 0;
}

bool TPlayerData::hasResourceBalance(const tibia::client::ResourceTypes_t type) const
{
    return m_resourcesBalance.find(type) != m_resourcesBalance.end();
}

uint64_t TPlayerData::totalMoney() const
{
    return resourceBalance(tibia::client::RESOURCE_BANK_BALANCE)
        + resourceBalance(tibia::client::RESOURCE_GOLD_EQUIPPED);
}

void TPlayerData::notifyPlayerDataChanged()
{
    Q_EMIT playerDataChanged();
}

void TPlayerData::setCharacterName(const std::string_view name)
{
    const std::string next(name);
    if (m_name == next)
        return;
    m_name = next;
    notifyPlayerDataChanged();
}

void TPlayerData::setHealth(const uint32_t health, const uint32_t maxHealth)
{
    if (m_health == health && m_maxHealth == maxHealth)
        return;
    const uint32_t oldHealth = m_health;
    const uint32_t oldMaxHealth = m_maxHealth;
    m_health = health;
    m_maxHealth = maxHealth;
    notifyPlayerDataChanged();
    notify("onHealthChange", health, maxHealth, oldHealth, oldMaxHealth);
    if (health == 0) {
        if (const LocalPlayerSessionThingPtr player = localPlayer()) {
            if (player->isPreWalking())
                player->stopWalk();
            player->lockWalk();
        }
    }
}

void TPlayerData::setMana(const uint32_t mana, const uint32_t maxMana)
{
    if (m_mana == mana && m_maxMana == maxMana)
        return;
    const uint32_t oldMana = m_mana;
    const uint32_t oldMaxMana = m_maxMana;
    m_mana = mana;
    m_maxMana = maxMana;
    notifyPlayerDataChanged();
    notify("onManaChange", mana, maxMana, oldMana, oldMaxMana);
}

void TPlayerData::setManaShield(const uint32_t manaShield, const uint32_t maxManaShield)
{
    if (m_manaShield == manaShield && m_maxManaShield == maxManaShield)
        return;
    const uint32_t oldManaShield = m_manaShield;
    const uint32_t oldMaxManaShield = m_maxManaShield;
    m_manaShield = manaShield;
    m_maxManaShield = maxManaShield;
    notifyPlayerDataChanged();
    notify("onManaShieldChange", manaShield, maxManaShield, oldManaShield, oldMaxManaShield);
}

void TPlayerData::setLevel(const uint16_t level, const uint16_t levelPercent)
{
    const bool levelUp = level > m_level && m_level != 0;
    const bool changed = m_level != level || m_levelPercent != levelPercent;
    if (!changed)
        return;
    const uint16_t oldLevel = m_level;
    const uint16_t oldLevelPercent = m_levelPercent;
    m_level = level;
    m_levelPercent = levelPercent;
    notifyPlayerDataChanged();
    notify("onLevelChange", level, levelPercent, oldLevel, oldLevelPercent);
    if (levelUp)
        Q_EMIT playerLevelUp();
}

void TPlayerData::setExperience(const uint64_t experience)
{
    if (m_experience == experience)
        return;
    const uint64_t oldExperience = m_experience;
    m_experience = experience;
    notifyPlayerDataChanged();
    notify("onExperienceChange", experience, oldExperience);
}

void TPlayerData::setStamina(const uint16_t stamina)
{
    if (m_stamina == stamina)
        return;
    const uint16_t oldStamina = m_stamina;
    m_stamina = stamina;
    notifyPlayerDataChanged();
    notify("onStaminaChange", stamina, oldStamina);
}

void TPlayerData::setVocation(const uint8_t vocation)
{
    if (m_vocation == vocation)
        return;
    const uint8_t oldVocation = m_vocation;
    m_vocation = vocation;
    notifyPlayerDataChanged();
    notify("onVocationChange", vocation, oldVocation);
    Q_EMIT vocationChanged();
}

void TPlayerData::setPremium(const bool premium)
{
    if (m_premium == premium)
        return;
    m_premium = premium;
    notifyPlayerDataChanged();
    notify("onPremiumChange", premium);
}

void TPlayerData::setMagicShieldVocation(const bool enabled)
{
    if (m_magicShieldVocation == enabled)
        return;
    m_magicShieldVocation = enabled;
    notifyPlayerDataChanged();
    Q_EMIT vocationSpecificPlayerDataChanged();
}

void TPlayerData::setHasReachedMain(const bool reached)
{
    if (m_hasReachedMain == reached)
        return;
    m_hasReachedMain = reached;
    notifyPlayerDataChanged();
    Q_EMIT vocationSpecificPlayerDataChanged();
}

void TPlayerData::setHarmony(const uint8_t harmony)
{
    if (m_harmony == harmony)
        return;
    const uint8_t oldHarmony = m_harmony;
    m_harmony = harmony;
    notifyPlayerDataChanged();
    notify("onHarmonyChange", harmony, oldHarmony);
}

void TPlayerData::setSerene(const bool serene)
{
    if (m_serene == serene)
        return;
    const bool oldSerene = m_serene;
    m_serene = serene;
    notifyPlayerDataChanged();
    notify("onSereneChange", serene, oldSerene);
}

void TPlayerData::setFreeCapacity(const uint32_t freeCapacity)
{
    if (m_freeCapacity == freeCapacity)
        return;
    const uint32_t oldFreeCapacity = m_freeCapacity;
    m_freeCapacity = freeCapacity;
    notifyPlayerDataChanged();
    notify("onFreeCapacityChange", freeCapacity, oldFreeCapacity);
}

void TPlayerData::setTotalCapacity(const uint32_t totalCapacity)
{
    if (m_totalCapacity == totalCapacity)
        return;
    const uint32_t oldTotalCapacity = m_totalCapacity;
    m_totalCapacity = totalCapacity;
    notifyPlayerDataChanged();
    notify("onTotalCapacityChange", totalCapacity, oldTotalCapacity);
}

void TPlayerData::setBaseCapacity(const uint32_t baseCapacity)
{
    if (m_baseCapacity == baseCapacity)
        return;
    m_baseCapacity = baseCapacity;
    notifyPlayerDataChanged();
    notify("onBaseCapacityChange", baseCapacity);
}

void TPlayerData::setStoreExpBoostPurchasable(const bool purchasable)
{
    if (m_storeExpBoostPurchasable == purchasable)
        return;
    m_storeExpBoostPurchasable = purchasable;
    notifyPlayerDataChanged();
    notify("onStoreExpBoostPurchasableChange", purchasable);
}

void TPlayerData::setResourceBalance(const tibia::client::ResourceTypes_t type, const uint64_t value)
{
    const auto it = m_resourcesBalance.find(type);
    const uint64_t oldBalance = it != m_resourcesBalance.end() ? it->second : 0;
    if (it != m_resourcesBalance.end() && value == oldBalance)
        return;
    m_resourcesBalance[type] = value;
    notifyPlayerDataChanged();
    notify("onResourcesBalanceChange", value, oldBalance, type);
    TGameNotify::instance()->emitResourcesBalanceChanged(static_cast<qint64>(value), static_cast<qint64>(oldBalance),
                                                         static_cast<int>(type));
}

void TPlayerData::setSpells(const std::vector<uint16_t>& spells)
{
    if (m_spells == spells)
        return;
    const std::vector<uint16_t> oldSpells = m_spells;
    m_spells = spells;
    notifyPlayerDataChanged();
    notify("onSpellsChange", spells, oldSpells);
}

void TPlayerData::setBlessings(const uint16_t blessings, const uint8_t blessVisualState)
{
    if (m_blessings == blessings && m_blessVisualState == blessVisualState)
        return;
    const uint16_t oldBlessings = m_blessings;
    m_blessings = blessings;
    m_blessVisualState = blessVisualState;
    notifyPlayerDataChanged();
    notify("onBlessingsChange", blessings, oldBlessings, blessVisualState);
}

void TPlayerData::setSoul(const uint8_t soul)
{
    if (m_soul == soul)
        return;
    const uint8_t oldSoul = m_soul;
    m_soul = soul;
    notifyPlayerDataChanged();
    notify("onSoulChange", soul, oldSoul);
}

void TPlayerData::setMagicLevel(const uint16_t magicLevel, const uint16_t magicLevelPercent)
{
    if (m_magicLevel == magicLevel && m_magicLevelPercent == magicLevelPercent)
        return;
    const uint16_t oldMagicLevel = m_magicLevel;
    const uint16_t oldMagicLevelPercent = m_magicLevelPercent;
    m_magicLevel = magicLevel;
    m_magicLevelPercent = magicLevelPercent;
    notifyPlayerDataChanged();
    notify("onMagicLevelChange", magicLevel, magicLevelPercent, oldMagicLevel, oldMagicLevelPercent);
}

void TPlayerData::setBaseMagicLevel(const uint16_t baseMagicLevel)
{
    if (m_baseMagicLevel == baseMagicLevel)
        return;
    const uint16_t oldBaseMagicLevel = m_baseMagicLevel;
    m_baseMagicLevel = baseMagicLevel;
    notifyPlayerDataChanged();
    notify("onBaseMagicLevelChange", baseMagicLevel, oldBaseMagicLevel);
}

uint16_t TPlayerData::skillLevel(const tibia::client::Skill skill) const
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    return m_skills[skill].level;
}

uint16_t TPlayerData::skillBaseLevel(const tibia::client::Skill skill) const
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    return m_skills[skill].baseLevel;
}

uint16_t TPlayerData::skillLoyaltyLevel(const tibia::client::Skill skill) const
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    return m_skills[skill].loyaltyLevel;
}

uint16_t TPlayerData::skillPercentHundredths(const tibia::client::Skill skill) const
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    return m_skills[skill].percentHundredths;
}

uint16_t TPlayerData::experienceRate(const tibia::client::ExperienceRate_t type) const
{
    const auto it = m_experienceRates.find(type);
    return it != m_experienceRates.end() ? it->second : 0;
}

void TPlayerData::setMagicLevelDetail(const uint16_t loyaltyLevel, const uint16_t percentHundredths)
{
    if (m_magicLevelLoyalty == loyaltyLevel && m_magicLevelPercentHundredths == percentHundredths)
        return;
    m_magicLevelLoyalty = loyaltyLevel;
    m_magicLevelPercentHundredths = percentHundredths;
}

void TPlayerData::setSkillLevel(const tibia::client::Skill skill, const uint16_t level,
                                const uint16_t levelPercent)
{
    if (skill >= tibia::client::LastSkill)
        return;
    auto& row = m_skills[skill];
    if (row.level == level && row.levelPercent == levelPercent)
        return;
    const uint16_t oldLevel = row.level;
    const uint16_t oldLevelPercent = row.levelPercent;
    row.level = level;
    row.levelPercent = levelPercent;
    notifyPlayerDataChanged();
    notify("onSkillChange", skill, level, levelPercent, oldLevel, oldLevelPercent);
}

void TPlayerData::setSkillDetail(const tibia::client::Skill skill, const uint16_t loyaltyLevel,
                                 const uint16_t percentHundredths)
{
    if (skill >= tibia::client::LastSkill)
        return;
    auto& row = m_skills[skill];
    row.loyaltyLevel = loyaltyLevel;
    row.percentHundredths = percentHundredths;
}

void TPlayerData::setSkillBaseLevel(const tibia::client::Skill skill, const uint16_t baseLevel)
{
    if (skill >= tibia::client::LastSkill)
        return;
    const uint16_t oldBaseLevel = m_skills[skill].baseLevel;
    if (oldBaseLevel == baseLevel)
        return;
    m_skills[skill].baseLevel = baseLevel;
    notifyPlayerDataChanged();
    notify("onBaseSkillChange", skill, baseLevel, oldBaseLevel);
}

void TPlayerData::setExperienceRate(const tibia::client::ExperienceRate_t type, const uint16_t value)
{
    const auto it = m_experienceRates.find(type);
    if (it != m_experienceRates.end() && it->second == value)
        return;
    m_experienceRates[type] = value;
    notifyPlayerDataChanged();
    notify("onExperienceRateChange", type, value);
}

void TPlayerData::setSpeed(const uint16_t speed)
{
    if (m_speed == speed)
        return;
    m_speed = speed;
}

void TPlayerData::setBaseSpeed(const uint16_t baseSpeed)
{
    if (m_baseSpeed == baseSpeed)
        return;
    const uint16_t oldBaseSpeed = m_baseSpeed;
    m_baseSpeed = baseSpeed;
    notifyPlayerDataChanged();
    notify("onBaseSpeedChange", baseSpeed, oldBaseSpeed);
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->setBaseSpeed(baseSpeed);
}

void TPlayerData::setRegenerationTime(const uint16_t regenerationTime)
{
    if (m_regenerationTime == regenerationTime)
        return;
    const uint16_t oldRegenerationTime = m_regenerationTime;
    m_regenerationTime = regenerationTime;
    notifyPlayerDataChanged();
    notify("onRegenerationChange", regenerationTime, oldRegenerationTime);
}

void TPlayerData::setOfflineTrainingTime(const uint16_t offlineTrainingTime)
{
    if (m_offlineTrainingTime == offlineTrainingTime)
        return;
    const uint16_t oldOfflineTrainingTime = m_offlineTrainingTime;
    m_offlineTrainingTime = offlineTrainingTime;
    notifyPlayerDataChanged();
    notify("onOfflineTrainingChange", offlineTrainingTime, oldOfflineTrainingTime);
}

void TPlayerData::setStoreExpBoostTime(const uint16_t storeExpBoostTime)
{
    if (m_storeExpBoostTime == storeExpBoostTime)
        return;
    m_storeExpBoostTime = storeExpBoostTime;
}

void TPlayerData::setRottenBloodState(const uint8_t rottenBloodState)
{
    if (m_rottenBloodState == rottenBloodState)
        return;
    m_rottenBloodState = rottenBloodState;
}

void TPlayerData::clear()
{
    m_name.clear();
    m_health = 0;
    m_maxHealth = 0;
    m_mana = 0;
    m_maxMana = 0;
    m_manaShield = 0;
    m_maxManaShield = 0;
    m_level = 0;
    m_levelPercent = 0;
    m_experience = 0;
    m_stamina = 0;
    m_vocation = 0;
    m_premium = false;
    m_magicShieldVocation = false;
    m_harmony = 0;
    m_serene = false;
    m_hasReachedMain = false;
    m_freeCapacity = 0;
    m_totalCapacity = 0;
    m_baseCapacity = 0;
    m_storeExpBoostPurchasable = false;
    m_blessVisualState = 0;
    m_blessings = tibia::client::BlessingNone;
    m_soul = 0;
    m_magicLevel = 0;
    m_magicLevelPercent = 0;
    m_baseMagicLevel = 0;
    m_magicLevelLoyalty = 0;
    m_magicLevelPercentHundredths = 0;
    m_spells.clear();
    m_skills = {};
    m_experienceRates.clear();
    m_speed = 0;
    m_baseSpeed = 0;
    m_regenerationTime = 0;
    m_offlineTrainingTime = 0;
    m_storeExpBoostTime = 0;
    m_rottenBloodState = 0;
    m_resourcesBalance.clear();
    notifyPlayerDataChanged();
}

}
