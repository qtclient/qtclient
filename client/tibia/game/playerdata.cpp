#include "game/playerdata.h"

#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "input/playermovementintenthandler.h"
#include <tibia/game/tgamenotify.h>
#include "input/gameaction.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

namespace tibia::game {
namespace {

/// The screenshot manager's low-health event.
constexpr int LowHealthScreenshot = 53;

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

/// The skill types of the skills message: magic level, shielding, distance,
/// club, sword, axe, fist, fishing.
constexpr int MagicLevelSkill = 4;

/// The skill type of a fighting skill or fishing; 0 for the others.
int skillTypeOf(const tibia::client::Skill skill)
{
    switch (skill) {
        case tibia::client::Fist: return 15;
        case tibia::client::Club: return 12;
        case tibia::client::Sword: return 13;
        case tibia::client::Axe: return 14;
        case tibia::client::Distance: return 11;
        case tibia::client::Shielding: return 10;
        case tibia::client::Fishing: return 16;
        default: return 0;
    }
}

/// The protocol's units of a skill: whole levels, the percent truncated or in
/// hundredths.
uint16_t wholeOf(const double value) { return static_cast<uint16_t>(value); }
uint16_t hundredthsOf(const double percent) { return static_cast<uint16_t>(std::lround(percent * 100.0)); }

} // namespace

TPlayerData::TPlayerData(QObject* parent)
    : cip::tibia::game::TPlayerData(parent)
    , m_skills{ { 4, {} }, { 14, {} }, { 12, {} }, { 11, {} }, { 15, {} }, { 10, {} },
                { 13, {} }, { 16, {} }, { 21, {} }, { 22, {} }, { 23, {} }, { 24, {} } }
{
    m_changeTimer.setInterval(0);
    m_changeTimer.setSingleShot(true);
    connect(&m_changeTimer, &QTimer::timeout, this, [this] { onChangeTimerElapsed(); });
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
    // An automatic low-health screenshot when the health percent fell below 4
    // since the last setHealth and the player is alive.
    const uint8_t percent = healthPercent();
    if (percent < 4 && percent < m_healthPercentBeforeChange && m_health != 0)
        Q_EMIT publishGameAction(std::make_shared<tibia::input::TGameActionTakeAutomaticScreenshot>(LowHealthScreenshot));
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

void TPlayerData::setExpertPvpMode(const bool enabled)
{
    if (m_expertPvpMode != enabled) {
        m_expertPvpMode = enabled;
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

void TPlayerData::setBlessings(TBlessings blessings)
{
    if (m_blessings != blessings) {
        m_blessings = std::move(blessings);
        scheduleChangeNotification();
    }
}

void TPlayerData::setBlessingsVisualState(const int visualState)
{
    if (m_blessingsVisualState != visualState) {
        m_blessingsVisualState = visualState;
        scheduleChangeNotification();
    }
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

TSkill TPlayerData::skill(const int skillType) const { return m_skills.at(skillType); }

void TPlayerData::setSkill(const int skillType, const double level, const double baseLevel, const double loyalty,
                           const double percent)
{
    const TSkill& current = m_skills.at(skillType);
    if (current.level == level && current.baseLevel == baseLevel && current.loyalty == loyalty && current.percent == percent)
        return;
    m_skills[skillType] = TSkill{ level, baseLevel, loyalty, percent };
    scheduleChangeNotification();
}

void TPlayerData::setSpecialMagicLevels(const std::vector<std::pair<int, int>>& specialMagicLevels)
{
    if (m_specialMagicLevels == specialMagicLevels)
        return;
    m_specialMagicLevels = specialMagicLevels;
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
    // The own walk ends with the character's death.
    if (health == 0) {
        if (const TGameSessionBase* session = currentGameSession()) {
            if (input::TPlayerMovementIntentHandler* intent = session->movementIntent())
                intent->stopWalking();
        }
    }
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
    setManaShield(static_cast<int>(manaShield));
    setMaxManaShield(static_cast<int>(maxManaShield));
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
    setExperience(static_cast<qint64>(experience));
}

void TPlayerData::updateStamina(const uint16_t stamina)
{
    setStaminaMinutes(stamina);
}

void TPlayerData::updateHarmony(const uint8_t harmony)
{
    setHarmony(harmony);
}

void TPlayerData::updateSerene(const bool serene)
{
    setSerene(serene);
}

void TPlayerData::updateFreeCapacity(const uint32_t hundredths)
{
    const uint32_t oldHundredths = m_freeCapacityHundredths;
    m_freeCapacityHundredths = hundredths;
    setFreeCapacity(static_cast<int>(hundredths / 100));
    // A change below one oz counts too (the inventory's zero-capacity warning).
    if (oldHundredths != hundredths)
        scheduleChangeNotification();
}

void TPlayerData::updateTotalCapacity(const uint32_t totalCapacity)
{
    setCapacity(static_cast<int>(totalCapacity));
}

void TPlayerData::updateBaseCapacity(const uint32_t baseCapacity)
{
    setBaseCapacity(static_cast<int>(baseCapacity));
}

void TPlayerData::updateStoreExpBoostPurchasable(const bool purchasable)
{
    setStoreXpBoostPurchasable(purchasable);
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
    TGameNotify::instance()->emitResourcesBalanceChanged(static_cast<qint64>(value), static_cast<qint64>(oldBalance),
                                                         static_cast<int>(type));
}

uint16_t TPlayerData::blessingMask() const
{
    uint16_t mask = tibia::client::BlessingNone;
    for (const int blessing : m_blessings)
        mask |= static_cast<uint16_t>(blessing);
    return mask;
}

void TPlayerData::updateBlessings(const uint16_t blessings, const uint8_t blessVisualState)
{
    const uint16_t oldBlessings = blessingMask();
    if (oldBlessings == blessings && m_blessingsVisualState == blessVisualState)
        return;
    TBlessings set;
    for (int bit = 0; bit < 16; ++bit) {
        if (blessings & (1u << bit))
            set.insert(1 << bit);
    }
    setBlessings(std::move(set));
    setBlessingsVisualState(blessVisualState);
    notify("onBlessingsChange", blessings, oldBlessings, blessVisualState);
}

void TPlayerData::updateSoul(const uint8_t soul)
{
    setSoul(soul);
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

uint16_t TPlayerData::magicLevel() const { return wholeOf(skill(MagicLevelSkill).level); }

uint16_t TPlayerData::magicLevelPercent() const { return wholeOf(skill(MagicLevelSkill).percent); }

uint16_t TPlayerData::baseMagicLevel() const { return wholeOf(skill(MagicLevelSkill).baseLevel); }

uint16_t TPlayerData::magicLevelLoyalty() const { return wholeOf(skill(MagicLevelSkill).loyalty); }

uint16_t TPlayerData::magicLevelPercentHundredths() const { return hundredthsOf(skill(MagicLevelSkill).percent); }

uint16_t TPlayerData::skillLevel(const tibia::client::Skill skill) const
{
    const int type = skillTypeOf(skill);
    return type ? wholeOf(this->skill(type).level) : 0;
}

uint16_t TPlayerData::skillBaseLevel(const tibia::client::Skill skill) const
{
    const int type = skillTypeOf(skill);
    return type ? wholeOf(this->skill(type).baseLevel) : 0;
}

uint16_t TPlayerData::skillLoyaltyLevel(const tibia::client::Skill skill) const
{
    const int type = skillTypeOf(skill);
    return type ? wholeOf(this->skill(type).loyalty) : 0;
}

uint16_t TPlayerData::skillPercentHundredths(const tibia::client::Skill skill) const
{
    const int type = skillTypeOf(skill);
    return type ? hundredthsOf(this->skill(type).percent) : 0;
}

void TPlayerData::updateSkill(const int skillType, const double level, const double baseLevel, const double loyalty,
                              const double percent)
{
    setSkill(skillType, level, baseLevel, loyalty, percent);
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
}

void TPlayerData::updateBaseSpeed(const uint16_t baseSpeed)
{
    setBaseSpeed(baseSpeed);
}

void TPlayerData::setRegenerationTime(const uint16_t regenerationTime)
{
    setRegenerationSeconds(regenerationTime);
}

void TPlayerData::setOfflineTrainingTime(const uint16_t offlineTrainingTime)
{
    setOfflineTrainingMinutes(offlineTrainingTime);
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
    m_freeCapacityHundredths = 0;
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
    m_expertPvpMode = false;
    m_pvpMode = 0;
    m_vocation = 0;
    m_hasReachedMain = false;
    m_blessings.clear();
    m_blessingsVisualState = 0;
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
    for (auto& entry : m_skills)
        entry.second = TSkill{};
    m_specialMagicLevels.clear();
    m_rottenBloodState = 0;
    m_otherResources.clear();
    m_reportedResourceTypes.clear();
    scheduleChangeNotification();
}

}
