#pragma once

#include <client/const.h>

#include <cip_gen.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace tibia {
namespace game {

/// Experience gain multipliers the server reports (1.0 is 100%).
struct TExperienceGainRates
{
    double base = 1.0;
    double lowLevelBonus = 0.0;
    double storeBoost = 0.0;
    double dailyRewardBoost = 0.0;
    double staminaFactor = 1.0;

    bool operator==(const TExperienceGainRates&) const = default;
};

/// The player's current and base speed. Setting either also copies the base
/// speed into `unmodifiedBaseSpeed` and clears `modifier`.
struct TPlayerSpeeds
{
    uint16_t speed = 0;
    uint16_t baseSpeed = 0;
    uint16_t unmodifiedBaseSpeed = 0;
    double modifier = 0.0;
};

/// The blessings a player holds (values of tibia::client::Blessings).
using TBlessings = std::set<int>;

/// The player-state flags (EPlayerStateFlag bits 0..29) and the counter the
/// state message carries along.
struct TPlayerStates
{
    std::bitset<30> flags;
    std::optional<int> counter;
};

/// CIP game::TPlayerData: the logged-in player's own data. Every setter stores
/// the value and, when it changed, starts a zero-interval single-shot timer, so
/// any number of changes in one event-loop pass end in one playerDataChanged.
/// The vocation and the monk's harmony/serene notify at once instead.
///
/// The second half of the class is the qtclient session API (`localPlayerSet*`
/// in gamesession): it converts protocol units, keeps the data CIP stores
/// elsewhere (skills, blessings, other resources) and fires the
/// product `notify` hooks.
class TPlayerData final : public cip::tibia::game::TPlayerData
{
    Q_OBJECT
public:
    explicit TPlayerData(QObject* parent = nullptr);

    static TPlayerData* liveInstance();
    static void setLiveInstance(TPlayerData* data);

    // ---- CIP TPlayerData ----

    int health() const { return m_health; }
    /// Remembers the health percent before the change for the low-health check
    /// that runs with the coalesced change notification.
    void setHealth(int health);
    int maxHealth() const { return m_maxHealth; }
    void setMaxHealth(int maxHealth);
    int freeCapacity() const { return m_freeCapacity; }
    /// The server's free capacity, in hundredths of an oz.
    uint32_t freeCapacityHundredths() const { return m_freeCapacityHundredths; }
    void setFreeCapacity(int freeCapacity);
    int capacity() const { return m_capacity; }
    void setCapacity(int capacity);
    int baseCapacity() const { return m_baseCapacity; }
    void setBaseCapacity(int baseCapacity);
    qint64 experience() const { return m_experience; }
    void setExperience(qint64 experience);
    uint16_t level() const { return m_level; }
    /// Emits playerLevelUp when a known level rises.
    void setLevel(uint16_t level);
    float levelPercent() const { return m_levelPercent; }
    void setLevelPercent(float levelPercent);
    const TExperienceGainRates& experienceGainRates() const { return m_experienceGainRates; }
    void setExperienceGainRates(const TExperienceGainRates& rates);
    int mana() const { return m_mana; }
    void setMana(int mana);
    int maxMana() const { return m_maxMana; }
    void setMaxMana(int maxMana);
    uint8_t soul() const { return m_soul; }
    void setSoul(uint8_t soul);
    TPlayerSpeeds speeds() const { return m_speeds; }
    void setSpeed(uint16_t speed);
    void setBaseSpeed(uint16_t baseSpeed);
    void setSpeeds(uint16_t speed, uint16_t baseSpeed);
    qint64 staminaMinutes() const { return m_staminaMinutes; }
    void setStaminaMinutes(qint64 minutes);
    qint64 regenerationSeconds() const { return m_regenerationSeconds; }
    void setRegenerationSeconds(qint64 seconds);
    qint64 offlineTrainingMinutes() const { return m_offlineTrainingMinutes; }
    void setOfflineTrainingMinutes(qint64 minutes);
    TPlayerStates states() const { return m_states; }
    void setStates(std::bitset<30> flags, std::optional<int> counter);
    bool isFighting() const { return m_states.flags.test(7); }
    bool isPowerless() const { return m_states.flags.test(28); }
    bool chaseOpponents() const { return m_chaseOpponents; }
    void setChaseOpponents(bool chase);
    bool secureMode() const { return m_secureMode; }
    void setSecureMode(bool secure);
    /// The world's expert PvP mode, as the login reports it.
    bool expertPvpMode() const { return m_expertPvpMode; }
    void setExpertPvpMode(bool enabled);
    int pvpMode() const { return m_pvpMode; }
    void setPvpMode(int pvpMode);
    uint8_t vocation() const { return m_vocation; }
    /// Emits vocationChanged at once (no coalescing).
    void setVocation(uint8_t vocation);
    bool hasReachedMain() const { return m_hasReachedMain; }
    /// Stores only; nothing is notified.
    void setHasReachedMain(bool reached);
    /// The blessings the player holds, each one bit of the protocol's
    /// blessing mask (1 the adventurer's blessing ... 256).
    TBlessings blessings() const { return m_blessings; }
    void setBlessings(TBlessings blessings);
    bool hasAdventurersBlessing() const { return m_blessings.contains(tibia::client::BlessingAdventurer); }
    int blessingsVisualState() const { return m_blessingsVisualState; }
    void setBlessingsVisualState(int visualState);
    qint64 storeXpBoostSeconds() const { return m_storeXpBoostSeconds; }
    void setStoreXpBoostSeconds(qint64 seconds);
    bool storeXpBoostPurchasable() const { return m_storeXpBoostPurchasable; }
    void setStoreXpBoostPurchasable(bool purchasable);
    qint64 dailyRewardXpBoostSeconds() const { return m_dailyRewardXpBoostSeconds; }
    void setDailyRewardXpBoostSeconds(qint64 seconds);
    qint64 bankGold() const { return m_bankGold; }
    void setBankGold(qint64 gold);
    qint64 inventoryGold() const { return m_inventoryGold; }
    void setInventoryGold(qint64 gold);
    /// Bank plus inventory gold.
    qint64 totalGold() const;
    qint64 guildBankGold() const { return m_guildBankGold; }
    /// Also marks the guild bank balance as known, changed or not.
    void setGuildBankGold(qint64 gold);
    bool isGuildBankGoldKnown() const { return m_guildBankGoldKnown; }
    bool promoted() const { return m_promoted; }
    void setPromoted(bool promoted);
    int manaShield() const { return m_manaShield; }
    void setManaShield(int manaShield);
    int maxManaShield() const { return m_maxManaShield; }
    void setMaxManaShield(int maxManaShield);
    bool magicShieldVocation() const { return m_magicShieldVocation; }
    void setMagicShieldVocation(bool enabled);
    const std::unordered_set<uint16_t>& knownSpells() const { return m_knownSpells; }
    void clearKnownSpells();
    void addKnownSpell(uint16_t spellId);
    /// 0..100; 0 while the maximum is 0.
    uint8_t healthPercent() const;
    uint8_t manaPercent() const;
    uint8_t manaShieldPercent() const;
    /// True while a change is waiting for its playerDataChanged.
    bool hasPendingChange() const { return m_changeTimer.isActive(); }
    uint8_t harmony() const { return m_harmony; }
    /// Emits vocationSpecificPlayerDataChanged at once.
    void setHarmony(uint8_t harmony);
    bool serene() const { return m_serene; }
    /// Emits vocationSpecificPlayerDataChanged at once.
    void setSerene(bool serene);

    // ---- qtclient session API ----

    const std::string& characterName() const { return m_name; }
    QString characterNameQString() const { return QString::fromStdString(m_name); }
    void setCharacterName(std::string_view name);

    /// Level progress in hundredths of a percent / whole percent.
    uint16_t levelPercentRaw() const;
    uint16_t stamina() const { return static_cast<uint16_t>(m_staminaMinutes); }
    uint32_t totalCapacity() const { return static_cast<uint32_t>(m_capacity); }
    bool storeExpBoostPurchasable() const { return m_storeXpBoostPurchasable; }
    uint8_t blessVisualState() const { return static_cast<uint8_t>(m_blessingsVisualState); }
    /// The blessings as the protocol's bit mask.
    uint16_t blessingMask() const;
    uint16_t magicLevel() const { return m_magicLevel; }
    uint16_t magicLevelPercent() const { return m_magicLevelPercent; }
    uint16_t baseMagicLevel() const { return m_baseMagicLevel; }
    uint16_t magicLevelLoyalty() const { return m_magicLevelLoyalty; }
    uint16_t magicLevelPercentHundredths() const { return m_magicLevelPercentHundredths; }
    /// The known spells in ascending id order.
    std::vector<uint16_t> spells() const;

    uint16_t skillLevel(tibia::client::Skill skill) const;
    uint16_t skillBaseLevel(tibia::client::Skill skill) const;
    uint16_t skillLoyaltyLevel(tibia::client::Skill skill) const;
    uint16_t skillPercentHundredths(tibia::client::Skill skill) const;
    /// The gain rate in percent (100 is 1.0); 0 for a rate the server does not send.
    uint16_t experienceRate(tibia::client::ExperienceRate_t type) const;
    uint16_t speed() const { return m_speeds.speed; }
    uint16_t baseSpeed() const { return m_speeds.baseSpeed; }
    uint16_t regenerationTime() const { return static_cast<uint16_t>(m_regenerationSeconds); }
    uint16_t offlineTrainingTime() const { return static_cast<uint16_t>(m_offlineTrainingMinutes); }
    uint16_t storeExpBoostTime() const { return static_cast<uint16_t>(m_storeXpBoostSeconds); }
    uint16_t dailyRewardExpBoostTime() const { return static_cast<uint16_t>(m_dailyRewardXpBoostSeconds); }
    uint8_t rottenBloodState() const { return m_rottenBloodState; }

    uint64_t resourceBalance(tibia::client::ResourceTypes_t type) const;
    bool hasResourceBalance(tibia::client::ResourceTypes_t type) const;
    uint64_t totalMoney() const { return static_cast<uint64_t>(totalGold()); }

    /// Legacy accessors of storeandresourcebalancehelpercontroller.
    qulonglong v72() const { return static_cast<qulonglong>(bankGold()); }
    qulonglong v77() const { return static_cast<qulonglong>(guildBankGold()); }
    bool v78() const { return isGuildBankGoldKnown(); }

    /// Product-facing updates: convert the protocol units, store through the
    /// CIP setters and fire the matching `notify` hook when the value changed.
    void updateHealth(uint32_t health, uint32_t maxHealth);
    void updateMana(uint32_t mana, uint32_t maxMana);
    void updateManaShield(uint32_t manaShield, uint32_t maxManaShield);
    /// `levelPercent` in hundredths of a percent.
    void updateLevel(uint16_t level, uint16_t levelPercent);
    void updateExperience(uint64_t experience);
    void updateStamina(uint16_t stamina);
    void updateHarmony(uint8_t harmony);
    void updateSerene(bool serene);
    /// `hundredths`: the server's free capacity in hundredths of an oz.
    void updateFreeCapacity(uint32_t hundredths);
    void updateTotalCapacity(uint32_t totalCapacity);
    void updateBaseCapacity(uint32_t baseCapacity);
    void updateStoreExpBoostPurchasable(bool purchasable);
    void setResourceBalance(tibia::client::ResourceTypes_t type, uint64_t value);
    /// `blessings`: the protocol's bit mask.
    void updateBlessings(uint16_t blessings, uint8_t blessVisualState = 0);
    void updateSoul(uint8_t soul);
    void setMagicLevel(uint16_t magicLevel, uint16_t magicLevelPercent);
    void setBaseMagicLevel(uint16_t baseMagicLevel);
    void setMagicLevelDetail(uint16_t loyaltyLevel, uint16_t percentHundredths);
    void setSkillLevel(tibia::client::Skill skill, uint16_t level, uint16_t levelPercent);
    void setSkillDetail(tibia::client::Skill skill, uint16_t loyaltyLevel, uint16_t percentHundredths);
    void setSkillBaseLevel(tibia::client::Skill skill, uint16_t baseLevel);
    void setExperienceRate(tibia::client::ExperienceRate_t type, uint16_t value);
    void updateBaseSpeed(uint16_t baseSpeed);
    void setRegenerationTime(uint16_t regenerationTime);
    void setOfflineTrainingTime(uint16_t offlineTrainingTime);
    void setStoreExpBoostTime(uint16_t storeExpBoostTime);
    void setDailyRewardExpBoostTime(uint16_t dailyRewardExpBoostTime);
    void setRottenBloodState(uint8_t rottenBloodState);

    void clear();

private:
    struct SkillSnapshot {
        uint16_t level{ 0 };
        uint16_t baseLevel{ 0 };
        uint16_t levelPercent{ 0 };
        uint16_t loyaltyLevel{ 0 };
        uint16_t percentHundredths{ 0 };
    };

    /// Starts the coalescing timer unless it already runs.
    void scheduleChangeNotification();
    void onChangeTimerElapsed();

    // CIP TPlayerData members, in layout order.
    int m_health = 0;
    int m_maxHealth = 0;
    int m_freeCapacity = 0;
    uint32_t m_freeCapacityHundredths = 0;
    int m_capacity = 0;
    int m_baseCapacity = 0;
    qint64 m_experience = 0;
    uint16_t m_level = 0;
    float m_levelPercent = 0.0f;
    TExperienceGainRates m_experienceGainRates;
    int m_mana = 0;
    int m_maxMana = 0;
    uint8_t m_soul = 0;
    TPlayerSpeeds m_speeds;
    qint64 m_staminaMinutes = 0;
    qint64 m_regenerationSeconds = 0;
    qint64 m_offlineTrainingMinutes = 0;
    TPlayerStates m_states;
    bool m_chaseOpponents = true;
    bool m_secureMode = true;
    bool m_expertPvpMode = false;
    int m_pvpMode = 0;
    uint8_t m_vocation = 0;
    bool m_hasReachedMain = false;
    TBlessings m_blessings;
    int m_blessingsVisualState = 0;
    qint64 m_storeXpBoostSeconds = 0;
    bool m_storeXpBoostPurchasable = false;
    qint64 m_dailyRewardXpBoostSeconds = 0;
    qint64 m_bankGold = 0;
    qint64 m_inventoryGold = 0;
    qint64 m_guildBankGold = 0;
    bool m_guildBankGoldKnown = false;
    bool m_promoted = false;
    uint8_t m_healthPercentBeforeChange = 0;
    int m_manaShield = 0;
    int m_maxManaShield = 0;
    bool m_magicShieldVocation = false;
    std::unordered_set<uint16_t> m_knownSpells;
    QTimer m_changeTimer;
    uint8_t m_harmony = 0;
    bool m_serene = false;

    // qtclient session data.
    std::string m_name;
    uint16_t m_magicLevel{ 0 };
    uint16_t m_magicLevelPercent{ 0 };
    uint16_t m_baseMagicLevel{ 0 };
    uint16_t m_magicLevelLoyalty{ 0 };
    uint16_t m_magicLevelPercentHundredths{ 0 };
    std::array<SkillSnapshot, tibia::client::LastSkill> m_skills{};
    uint8_t m_rottenBloodState{ 0 };
    std::map<tibia::client::ResourceTypes_t, uint64_t> m_otherResources;
    /// Resource types reported at least once (the product hook fires on the first report).
    std::set<tibia::client::ResourceTypes_t> m_reportedResourceTypes;
};

}
}
