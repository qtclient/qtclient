#include "game/playerdata.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

#include <QtMath>

#include <algorithm>
#include <string_view>

namespace tibia::game {
namespace {

TPlayerData* g_livePlayerData = nullptr;

/// Percent of `value` in `maximum`, capped at 100; 0 while the maximum is 0.
uint8_t percentOf(const int value, const int maximum)
{
    const auto scaled = static_cast<uint32_t>(value * 100);
    const auto max = static_cast<uint32_t>(maximum);
    uint32_t percent = 0;
    if (max != 0) {
        percent = std::min<uint32_t>(scaled / max, 100);
        if (scaled < max)
            percent = 0;
    }
    return static_cast<uint8_t>(percent);
}

} // namespace

TPlayerData::TPlayerData(QObject* parent)
    : cip::tibia::game::TPlayerData(parent)
{
    m_changeTimer.setInterval(0);
    m_changeTimer.setSingleShot(true);
    connect(&m_changeTimer, &QTimer::timeout, this, [this] { onChangeTimerElapsed(); });
}

TPlayerData* TPlayerData::liveInstance()
{
    return g_livePlayerData;
}

void TPlayerData::setLiveInstance(TPlayerData* data)
{
    g_livePlayerData = data;
}

// ---- CIP TPlayerData ----

void TPlayerData::scheduleChangeNotification()
{
    if (!m_changeTimer.isActive())
        m_changeTimer.start();
}

void TPlayerData::onChangeTimerElapsed()
{
    Q_EMIT playerDataChanged();
    // CIP also publishes an automatic low-health screenshot when the health
    // percent fell below 4 since the last setHealth and the player is alive;
    // qtclient has no automatic-screenshot kind to publish yet.
}

void TPlayerData::setHealth(const int health)
{
    m_healthPercentBeforeChange = healthPercent();
    if (m_health != health) {
        m_health = health;
        scheduleChangeNotification();
    }
}

void TPlayerData::setMaxHealth(const int maxHealth)
{
    if (m_maxHealth != maxHealth) {
        m_maxHealth = maxHealth;
        scheduleChangeNotification();
    }
}

void TPlayerData::setFreeCapacity(const int freeCapacity)
{
    if (m_freeCapacity != freeCapacity) {
        m_freeCapacity = freeCapacity;
        scheduleChangeNotification();
    }
}

void TPlayerData::setCapacity(const int capacity)
{
    if (m_capacity != capacity) {
        m_capacity = capacity;
        scheduleChangeNotification();
    }
}

void TPlayerData::setBaseCapacity(const int baseCapacity)
{
    if (m_baseCapacity != baseCapacity) {
        m_baseCapacity = baseCapacity;
        scheduleChangeNotification();
    }
}

void TPlayerData::setExperience(const qint64 experience)
{
    if (m_experience != experience) {
        m_experience = experience;
        scheduleChangeNotification();
    }
}

void TPlayerData::setLevel(const uint16_t level)
{
    const uint16_t previous = m_level;
    if (previous != level) {
        m_level = level;
        if (previous != 0 && previous < level)
            Q_EMIT playerLevelUp();
        scheduleChangeNotification();
    }
}

void TPlayerData::setLevelPercent(const float levelPercent)
{
    if (m_levelPercent != levelPercent) {
        m_levelPercent = levelPercent;
        scheduleChangeNotification();
    }
}

void TPlayerData::setExperienceGainRates(const TExperienceGainRates& rates)
{
    if (m_experienceGainRates == rates)
        return;
    m_experienceGainRates = rates;
    scheduleChangeNotification();
}

void TPlayerData::setMana(const int mana)
{
    if (m_mana != mana) {
        m_mana = mana;
        scheduleChangeNotification();
    }
}

void TPlayerData::setMaxMana(const int maxMana)
{
    if (m_maxMana != maxMana) {
        m_maxMana = maxMana;
        scheduleChangeNotification();
    }
}

void TPlayerData::setSoul(const uint8_t soul)
{
    if (m_soul != soul) {
        m_soul = soul;
        scheduleChangeNotification();
    }
}

void TPlayerData::setSpeed(const uint16_t speed)
{
    setSpeeds(speed, m_speeds.baseSpeed);
}

void TPlayerData::setBaseSpeed(const uint16_t baseSpeed)
{
    setSpeeds(m_speeds.speed, baseSpeed);
}

void TPlayerData::setSpeeds(const uint16_t speed, const uint16_t baseSpeed)
{
    if (m_speeds.speed == speed && m_speeds.baseSpeed == baseSpeed && m_speeds.unmodifiedBaseSpeed == baseSpeed
        && m_speeds.modifier == 0.0)
        return;
    m_speeds.speed = speed;
    m_speeds.baseSpeed = baseSpeed;
    m_speeds.unmodifiedBaseSpeed = baseSpeed;
    m_speeds.modifier = 0.0;
    scheduleChangeNotification();
}

void TPlayerData::setStaminaMinutes(const qint64 minutes)
{
    if (m_staminaMinutes != minutes) {
        m_staminaMinutes = minutes;
        scheduleChangeNotification();
    }
}

void TPlayerData::setRegenerationSeconds(const qint64 seconds)
{
    if (m_regenerationSeconds != seconds) {
        m_regenerationSeconds = seconds;
        scheduleChangeNotification();
    }
}

void TPlayerData::setOfflineTrainingMinutes(const qint64 minutes)
{
    if (m_offlineTrainingMinutes != minutes) {
        m_offlineTrainingMinutes = minutes;
        scheduleChangeNotification();
    }
}

void TPlayerData::setStates(const std::bitset<30> flags, const std::optional<int> counter)
{
    if (m_states.flags == flags && m_states.counter == counter)
        return;
    m_states.flags = flags;
    m_states.counter = counter;
    scheduleChangeNotification();
}

void TPlayerData::setChaseOpponents(const bool chase)
{
    if (m_chaseOpponents != chase) {
        m_chaseOpponents = chase;
        scheduleChangeNotification();
    }
}

void TPlayerData::setSecureMode(const bool secure)
{
    if (m_secureMode != secure) {
        m_secureMode = secure;
        scheduleChangeNotification();
    }
}

void TPlayerData::setPvpMode(const int pvpMode)
{
    if (m_pvpMode != pvpMode) {
        m_pvpMode = pvpMode;
        scheduleChangeNotification();
    }
}

void TPlayerData::setVocation(const uint8_t vocation)
{
    if (m_vocation == vocation)
        return;
    m_vocation = vocation;
    Q_EMIT vocationChanged();
}

void TPlayerData::setHasReachedMain(const bool reached)
{
    m_hasReachedMain = reached;
}

void TPlayerData::setStoreXpBoostSeconds(const qint64 seconds)
{
    if (m_storeXpBoostSeconds != seconds) {
        m_storeXpBoostSeconds = seconds;
        scheduleChangeNotification();
    }
}

void TPlayerData::setStoreXpBoostPurchasable(const bool purchasable)
{
    if (m_storeXpBoostPurchasable != purchasable) {
        m_storeXpBoostPurchasable = purchasable;
        scheduleChangeNotification();
    }
}

void TPlayerData::setDailyRewardXpBoostSeconds(const qint64 seconds)
{
    if (m_dailyRewardXpBoostSeconds != seconds) {
        m_dailyRewardXpBoostSeconds = seconds;
        scheduleChangeNotification();
    }
}

void TPlayerData::setBankGold(const qint64 gold)
{
    if (m_bankGold != gold) {
        m_bankGold = gold;
        scheduleChangeNotification();
    }
}

void TPlayerData::setInventoryGold(const qint64 gold)
{
    if (m_inventoryGold != gold) {
        m_inventoryGold = gold;
        scheduleChangeNotification();
    }
}

qint64 TPlayerData::totalGold() const
{
    return m_inventoryGold + m_bankGold;
}

void TPlayerData::setGuildBankGold(const qint64 gold)
{
    if (m_guildBankGold != gold) {
        m_guildBankGold = gold;
        scheduleChangeNotification();
    }
    m_guildBankGoldKnown = true;
}

void TPlayerData::setPromoted(const bool promoted)
{
    if (m_promoted != promoted) {
        m_promoted = promoted;
        scheduleChangeNotification();
    }
}

void TPlayerData::setManaShield(const int manaShield)
{
    if (m_manaShield != manaShield) {
        m_manaShield = manaShield;
        scheduleChangeNotification();
    }
}

void TPlayerData::setMaxManaShield(const int maxManaShield)
{
    if (m_maxManaShield != maxManaShield) {
        m_maxManaShield = maxManaShield;
        scheduleChangeNotification();
    }
}

void TPlayerData::setMagicShieldVocation(const bool enabled)
{
    if (m_magicShieldVocation != enabled) {
        m_magicShieldVocation = enabled;
        scheduleChangeNotification();
    }
}

void TPlayerData::clearKnownSpells()
{
    m_knownSpells.clear();
    scheduleChangeNotification();
}

void TPlayerData::addKnownSpell(const uint16_t spellId)
{
    m_knownSpells.insert(spellId);
    scheduleChangeNotification();
}

uint8_t TPlayerData::healthPercent() const
{
    return percentOf(health(), maxHealth());
}

uint8_t TPlayerData::manaPercent() const
{
    return percentOf(mana(), maxMana());
}

uint8_t TPlayerData::manaShieldPercent() const
{
    return percentOf(manaShield(), maxManaShield());
}

void TPlayerData::setHarmony(const uint8_t harmony)
{
    if (m_harmony == harmony)
        return;
    m_harmony = harmony;
    Q_EMIT vocationSpecificPlayerDataChanged();
}

void TPlayerData::setSerene(const bool serene)
{
    if (m_serene == serene)
        return;
    m_serene = serene;
    Q_EMIT vocationSpecificPlayerDataChanged();
}

// ---- qtclient session API ----

void TPlayerData::setCharacterName(const std::string_view name)
{
    const std::string next(name);
    if (m_name == next)
        return;
    m_name = next;
    scheduleChangeNotification();
}

uint16_t TPlayerData::levelPercentRaw() const
{
    return static_cast<uint16_t>(qRound(m_levelPercent * 100.0f));
}

std::vector<uint16_t> TPlayerData::spells() const
{
    std::vector<uint16_t> ids(m_knownSpells.begin(), m_knownSpells.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

uint64_t TPlayerData::resourceBalance(const tibia::client::ResourceTypes_t type) const
{
    switch (type) {
        case tibia::client::RESOURCE_BANK_BALANCE:
            return static_cast<uint64_t>(m_bankGold);
        case tibia::client::RESOURCE_GOLD_EQUIPPED:
            return static_cast<uint64_t>(m_inventoryGold);
        case tibia::client::RESOURCE_GUILD_BANK_BALANCE:
            return static_cast<uint64_t>(m_guildBankGold);
        default:
            break;
    }
    const auto it = m_otherResources.find(type);
    return it != m_otherResources.end() ? it->second : 0;
}

bool TPlayerData::hasResourceBalance(const tibia::client::ResourceTypes_t type) const
{
    switch (type) {
        case tibia::client::RESOURCE_BANK_BALANCE:
        case tibia::client::RESOURCE_GOLD_EQUIPPED:
            return true;
        case tibia::client::RESOURCE_GUILD_BANK_BALANCE:
            return m_guildBankGoldKnown;
        default:
            return m_otherResources.find(type) != m_otherResources.end();
    }
}

void TPlayerData::updateHealth(const uint32_t health, const uint32_t maxHealth)
{
    const auto oldHealth = static_cast<uint32_t>(m_health);
    const auto oldMaxHealth = static_cast<uint32_t>(m_maxHealth);
    setHealth(static_cast<int>(health));
    setMaxHealth(static_cast<int>(maxHealth));
    if (oldHealth == health && oldMaxHealth == maxHealth)
        return;
    notify("onHealthChange", health, maxHealth, oldHealth, oldMaxHealth);
    if (health == 0)
        tibia::game::localPlayerStopAutoWalk();
}

void TPlayerData::updateMana(const uint32_t mana, const uint32_t maxMana)
{
    const auto oldMana = static_cast<uint32_t>(m_mana);
    const auto oldMaxMana = static_cast<uint32_t>(m_maxMana);
    setMana(static_cast<int>(mana));
    setMaxMana(static_cast<int>(maxMana));
    if (oldMana != mana || oldMaxMana != maxMana)
        notify("onManaChange", mana, maxMana, oldMana, oldMaxMana);
}

void TPlayerData::updateManaShield(const uint32_t manaShield, const uint32_t maxManaShield)
{
    const auto oldManaShield = static_cast<uint32_t>(m_manaShield);
    const auto oldMaxManaShield = static_cast<uint32_t>(m_maxManaShield);
    setManaShield(static_cast<int>(manaShield));
    setMaxManaShield(static_cast<int>(maxManaShield));
    if (oldManaShield != manaShield || oldMaxManaShield != maxManaShield)
        notify("onManaShieldChange", manaShield, maxManaShield, oldManaShield, oldMaxManaShield);
}

void TPlayerData::updateLevel(const uint16_t level, const uint16_t levelPercent)
{
    const uint16_t oldLevel = m_level;
    const uint16_t oldLevelPercent = levelPercentRaw();
    setLevel(level);
    setLevelPercent(static_cast<float>(levelPercent) / 100.0f);
    if (oldLevel != level || oldLevelPercent != levelPercent)
        notify("onLevelChange", level, levelPercent, oldLevel, oldLevelPercent);
}

void TPlayerData::updateExperience(const uint64_t experience)
{
    const auto oldExperience = static_cast<uint64_t>(m_experience);
    setExperience(static_cast<qint64>(experience));
    if (oldExperience != experience)
        notify("onExperienceChange", experience, oldExperience);
}

void TPlayerData::updateStamina(const uint16_t stamina)
{
    const uint16_t oldStamina = this->stamina();
    setStaminaMinutes(stamina);
    if (oldStamina != stamina)
        notify("onStaminaChange", stamina, oldStamina);
}

void TPlayerData::updateHarmony(const uint8_t harmony)
{
    const uint8_t oldHarmony = m_harmony;
    setHarmony(harmony);
    if (oldHarmony != harmony)
        notify("onHarmonyChange", harmony, oldHarmony);
}

void TPlayerData::updateSerene(const bool serene)
{
    const bool oldSerene = m_serene;
    setSerene(serene);
    if (oldSerene != serene)
        notify("onSereneChange", serene, oldSerene);
}

void TPlayerData::updateFreeCapacity(const uint32_t freeCapacity)
{
    const auto oldFreeCapacity = static_cast<uint32_t>(m_freeCapacity);
    setFreeCapacity(static_cast<int>(freeCapacity));
    if (oldFreeCapacity != freeCapacity)
        notify("onFreeCapacityChange", freeCapacity, oldFreeCapacity);
}

void TPlayerData::updateTotalCapacity(const uint32_t totalCapacity)
{
    const auto oldTotalCapacity = static_cast<uint32_t>(m_capacity);
    setCapacity(static_cast<int>(totalCapacity));
    if (oldTotalCapacity != totalCapacity)
        notify("onTotalCapacityChange", totalCapacity, oldTotalCapacity);
}

void TPlayerData::updateBaseCapacity(const uint32_t baseCapacity)
{
    const auto oldBaseCapacity = static_cast<uint32_t>(m_baseCapacity);
    setBaseCapacity(static_cast<int>(baseCapacity));
    if (oldBaseCapacity != baseCapacity)
        notify("onBaseCapacityChange", baseCapacity);
}

void TPlayerData::updateStoreExpBoostPurchasable(const bool purchasable)
{
    const bool old = m_storeXpBoostPurchasable;
    setStoreXpBoostPurchasable(purchasable);
    if (old != purchasable)
        notify("onStoreExpBoostPurchasableChange", purchasable);
}

void TPlayerData::setResourceBalance(const tibia::client::ResourceTypes_t type, const uint64_t value)
{
    const bool reported = m_reportedResourceTypes.count(type) != 0;
    const uint64_t oldBalance = reported ? resourceBalance(type) : 0;
    switch (type) {
        case tibia::client::RESOURCE_BANK_BALANCE:
            setBankGold(static_cast<qint64>(value));
            break;
        case tibia::client::RESOURCE_GOLD_EQUIPPED:
            setInventoryGold(static_cast<qint64>(value));
            break;
        case tibia::client::RESOURCE_GUILD_BANK_BALANCE:
            setGuildBankGold(static_cast<qint64>(value));
            break;
        default:
            if (!reported || oldBalance != value) {
                m_otherResources[type] = value;
                scheduleChangeNotification();
            }
            break;
    }
    m_reportedResourceTypes.insert(type);
    if (reported && value == oldBalance)
        return;
    notify("onResourcesBalanceChange", value, oldBalance, type);
    TGameNotify::instance()->emitResourcesBalanceChanged(static_cast<qint64>(value), static_cast<qint64>(oldBalance),
                                                         static_cast<int>(type));
}

void TPlayerData::setBlessings(const uint16_t blessings, const uint8_t blessVisualState)
{
    if (m_blessings == blessings && m_blessVisualState == blessVisualState)
        return;
    const uint16_t oldBlessings = m_blessings;
    m_blessings = blessings;
    m_blessVisualState = blessVisualState;
    scheduleChangeNotification();
    notify("onBlessingsChange", blessings, oldBlessings, blessVisualState);
}

void TPlayerData::updateSoul(const uint8_t soul)
{
    const uint8_t oldSoul = m_soul;
    setSoul(soul);
    if (oldSoul != soul)
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
    scheduleChangeNotification();
    notify("onMagicLevelChange", magicLevel, magicLevelPercent, oldMagicLevel, oldMagicLevelPercent);
}

void TPlayerData::setBaseMagicLevel(const uint16_t baseMagicLevel)
{
    if (m_baseMagicLevel == baseMagicLevel)
        return;
    const uint16_t oldBaseMagicLevel = m_baseMagicLevel;
    m_baseMagicLevel = baseMagicLevel;
    scheduleChangeNotification();
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
    const TExperienceGainRates& rates = m_experienceGainRates;
    double rate = 0.0;
    switch (type) {
        case tibia::client::EXP_BASE: rate = rates.base; break;
        case tibia::client::EXP_LOWLEVEL: rate = rates.lowLevelBonus; break;
        case tibia::client::EXP_XPBOOST: rate = rates.storeBoost; break;
        case tibia::client::EXP_DAILYREWARD: rate = rates.dailyRewardBoost; break;
        case tibia::client::EXP_STANINAMULTIPLIER: rate = rates.staminaFactor; break;
        default: break;
    }
    return static_cast<uint16_t>(qRound(rate * 100.0));
}

void TPlayerData::setMagicLevelDetail(const uint16_t loyaltyLevel, const uint16_t percentHundredths)
{
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
    scheduleChangeNotification();
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
    scheduleChangeNotification();
    notify("onBaseSkillChange", skill, baseLevel, oldBaseLevel);
}

void TPlayerData::setExperienceRate(const tibia::client::ExperienceRate_t type, const uint16_t value)
{
    TExperienceGainRates rates = m_experienceGainRates;
    const double rate = value / 100.0;
    switch (type) {
        case tibia::client::EXP_BASE: rates.base = rate; break;
        case tibia::client::EXP_LOWLEVEL: rates.lowLevelBonus = rate; break;
        case tibia::client::EXP_XPBOOST: rates.storeBoost = rate; break;
        case tibia::client::EXP_DAILYREWARD: rates.dailyRewardBoost = rate; break;
        case tibia::client::EXP_STANINAMULTIPLIER: rates.staminaFactor = rate; break;
        default: return;
    }
    if (rates == m_experienceGainRates)
        return;
    setExperienceGainRates(rates);
    notify("onExperienceRateChange", type, value);
}

void TPlayerData::updateBaseSpeed(const uint16_t baseSpeed)
{
    const uint16_t oldBaseSpeed = m_speeds.baseSpeed;
    setBaseSpeed(baseSpeed);
    if (oldBaseSpeed == baseSpeed)
        return;
    notify("onBaseSpeedChange", baseSpeed, oldBaseSpeed);
}

void TPlayerData::setRegenerationTime(const uint16_t regenerationTime)
{
    const uint16_t oldRegenerationTime = this->regenerationTime();
    setRegenerationSeconds(regenerationTime);
    if (oldRegenerationTime != regenerationTime)
        notify("onRegenerationChange", regenerationTime, oldRegenerationTime);
}

void TPlayerData::setOfflineTrainingTime(const uint16_t offlineTrainingTime)
{
    const uint16_t oldOfflineTrainingTime = this->offlineTrainingTime();
    setOfflineTrainingMinutes(offlineTrainingTime);
    if (oldOfflineTrainingTime != offlineTrainingTime)
        notify("onOfflineTrainingChange", offlineTrainingTime, oldOfflineTrainingTime);
}

void TPlayerData::setStoreExpBoostTime(const uint16_t storeExpBoostTime)
{
    setStoreXpBoostSeconds(storeExpBoostTime);
}

void TPlayerData::setDailyRewardExpBoostTime(const uint16_t dailyRewardExpBoostTime)
{
    setDailyRewardXpBoostSeconds(dailyRewardExpBoostTime);
}

void TPlayerData::setRottenBloodState(const uint8_t rottenBloodState)
{
    m_rottenBloodState = rottenBloodState;
}

void TPlayerData::clear()
{
    m_health = 0;
    m_maxHealth = 0;
    m_freeCapacity = 0;
    m_capacity = 0;
    m_baseCapacity = 0;
    m_experience = 0;
    m_level = 0;
    m_levelPercent = 0.0f;
    m_experienceGainRates = {};
    m_mana = 0;
    m_maxMana = 0;
    m_soul = 0;
    m_speeds = {};
    m_staminaMinutes = 0;
    m_regenerationSeconds = 0;
    m_offlineTrainingMinutes = 0;
    m_states = {};
    m_chaseOpponents = true;
    m_secureMode = true;
    m_pvpMode = 0;
    m_vocation = 0;
    m_hasReachedMain = false;
    m_storeXpBoostSeconds = 0;
    m_storeXpBoostPurchasable = false;
    m_dailyRewardXpBoostSeconds = 0;
    m_bankGold = 0;
    m_inventoryGold = 0;
    m_guildBankGold = 0;
    m_guildBankGoldKnown = false;
    m_promoted = false;
    m_healthPercentBeforeChange = 0;
    m_manaShield = 0;
    m_maxManaShield = 0;
    m_magicShieldVocation = false;
    m_knownSpells.clear();
    m_harmony = 0;
    m_serene = false;

    m_name.clear();
    m_blessVisualState = 0;
    m_blessings = tibia::client::BlessingNone;
    m_magicLevel = 0;
    m_magicLevelPercent = 0;
    m_baseMagicLevel = 0;
    m_magicLevelLoyalty = 0;
    m_magicLevelPercentHundredths = 0;
    m_skills = {};
    m_rottenBloodState = 0;
    m_otherResources.clear();
    m_reportedResourceTypes.clear();
    scheduleChangeNotification();
}

}
