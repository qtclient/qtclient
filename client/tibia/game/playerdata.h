#pragma once

#include <client/const.h>

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tibia {
namespace game {

/// CIP game::TPlayerData — session vitals/resources (signal playerDataChanged).
/// Protocol vitals authority (gamesession setters). Map keep-alive reads peels + TCreature.
class TPlayerData final : public cip::tibia::game::TPlayerData
{
    Q_OBJECT
public:
    explicit TPlayerData(QObject* parent = nullptr);

    static TPlayerData* liveInstance();
    static void setLiveInstance(TPlayerData* data);

    void name();

    // Tip resource/scalar peels (not in External FUNCMAP as these names — no b1).
    // Keep call-site names until demangle maps them to stamped vN.
    void v71(qulonglong value);
    qulonglong v72() const;
    void v73(qulonglong value);
    qulonglong v75() const;
    void v76(qulonglong value);
    qulonglong v77() const;
    bool v78() const;
    void v91(long value);
    long v92() const;
    void v93(long value);
    long v94() const;
    void v95(long value);
    long v96() const;

    const std::string& characterName() const { return m_name; }
    QString characterNameQString() const { return QString::fromStdString(m_name); }

    void setCharacterName(std::string_view name);

    uint32_t health() const { return m_health; }
    uint32_t maxHealth() const { return m_maxHealth; }
    uint32_t mana() const { return m_mana; }
    uint32_t maxMana() const { return m_maxMana; }
    uint32_t manaShield() const { return m_manaShield; }
    uint32_t maxManaShield() const { return m_maxManaShield; }
    uint16_t level() const { return m_level; }
    uint16_t levelPercentRaw() const { return m_levelPercent; }
    uint16_t levelPercent() const;
    uint64_t experience() const { return m_experience; }
    uint16_t stamina() const { return m_stamina; }
    uint8_t vocation() const { return m_vocation; }
    bool premium() const { return m_premium; }
    bool magicShieldVocation() const { return m_magicShieldVocation; }
    uint8_t harmony() const { return m_harmony; }
    bool serene() const { return m_serene; }
    bool hasReachedMain() const { return m_hasReachedMain; }
    uint32_t freeCapacity() const { return m_freeCapacity; }
    uint32_t totalCapacity() const { return m_totalCapacity; }
    uint32_t baseCapacity() const { return m_baseCapacity; }
    bool storeExpBoostPurchasable() const { return m_storeExpBoostPurchasable; }
    uint8_t blessVisualState() const { return m_blessVisualState; }
    uint16_t blessings() const { return m_blessings; }
    uint8_t soul() const { return m_soul; }
    uint16_t magicLevel() const { return m_magicLevel; }
    uint16_t magicLevelPercent() const { return m_magicLevelPercent; }
    uint16_t baseMagicLevel() const { return m_baseMagicLevel; }
    uint16_t magicLevelLoyalty() const { return m_magicLevelLoyalty; }
    uint16_t magicLevelPercentHundredths() const { return m_magicLevelPercentHundredths; }
    const std::vector<uint16_t>& spells() const { return m_spells; }

    uint16_t skillLevel(tibia::client::Skill skill) const;
    uint16_t skillBaseLevel(tibia::client::Skill skill) const;
    uint16_t skillLoyaltyLevel(tibia::client::Skill skill) const;
    uint16_t skillPercentHundredths(tibia::client::Skill skill) const;
    uint16_t experienceRate(tibia::client::ExperienceRate_t type) const;
    uint16_t speed() const { return m_speed; }
    uint16_t baseSpeed() const { return m_baseSpeed; }
    uint16_t regenerationTime() const { return m_regenerationTime; }
    uint16_t offlineTrainingTime() const { return m_offlineTrainingTime; }
    uint16_t storeExpBoostTime() const { return m_storeExpBoostTime; }
    uint16_t dailyRewardExpBoostTime() const { return m_dailyRewardExpBoostTime; }
    uint8_t rottenBloodState() const { return m_rottenBloodState; }

    uint64_t resourceBalance(tibia::client::ResourceTypes_t type) const;
    bool hasResourceBalance(tibia::client::ResourceTypes_t type) const;
    uint64_t totalMoney() const;

    void setHealth(uint32_t health, uint32_t maxHealth);
    void setMana(uint32_t mana, uint32_t maxMana);
    void setManaShield(uint32_t manaShield, uint32_t maxManaShield);
    void setLevel(uint16_t level, uint16_t levelPercent);
    void setExperience(uint64_t experience);
    void setStamina(uint16_t stamina);
    void setVocation(uint8_t vocation);
    void setPremium(bool premium);
    void setMagicShieldVocation(bool enabled);
    void setHasReachedMain(bool reached);
    void setHarmony(uint8_t harmony);
    void setSerene(bool serene);
    void setFreeCapacity(uint32_t freeCapacity);
    void setTotalCapacity(uint32_t totalCapacity);
    void setBaseCapacity(uint32_t baseCapacity);
    void setStoreExpBoostPurchasable(bool purchasable);
    void setResourceBalance(tibia::client::ResourceTypes_t type, uint64_t value);
    void setSpells(const std::vector<uint16_t>& spells);
    void setBlessings(uint16_t blessings, uint8_t blessVisualState = 0);
    void setSoul(uint8_t soul);
    void setMagicLevel(uint16_t magicLevel, uint16_t magicLevelPercent);
    void setBaseMagicLevel(uint16_t baseMagicLevel);
    void setMagicLevelDetail(uint16_t loyaltyLevel, uint16_t percentHundredths);
    void setSkillLevel(tibia::client::Skill skill, uint16_t level, uint16_t levelPercent);
    void setSkillDetail(tibia::client::Skill skill, uint16_t loyaltyLevel, uint16_t percentHundredths);
    void setSkillBaseLevel(tibia::client::Skill skill, uint16_t baseLevel);
    void setExperienceRate(tibia::client::ExperienceRate_t type, uint16_t value);
    void setSpeed(uint16_t speed);
    void setBaseSpeed(uint16_t baseSpeed);
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

    void notifyPlayerDataChanged();

    std::string m_name;
    uint32_t m_health{ 0 };
    uint32_t m_maxHealth{ 0 };
    uint32_t m_mana{ 0 };
    uint32_t m_maxMana{ 0 };
    uint32_t m_manaShield{ 0 };
    uint32_t m_maxManaShield{ 0 };
    uint16_t m_level{ 0 };
    uint16_t m_levelPercent{ 0 };
    uint64_t m_experience{ 0 };
    uint16_t m_stamina{ 0 };
    uint8_t m_vocation{ 0 };
    bool m_premium{ false };
    bool m_magicShieldVocation{ false };
    uint8_t m_harmony{ 0 };
    bool m_serene{ false };
    bool m_hasReachedMain{ false };
    uint32_t m_freeCapacity{ 0 };
    uint32_t m_totalCapacity{ 0 };
    uint32_t m_baseCapacity{ 0 };
    bool m_storeExpBoostPurchasable{ false };
    uint8_t m_blessVisualState{ 0 };
    uint16_t m_blessings{ tibia::client::BlessingNone };
    uint8_t m_soul{ 0 };
    uint16_t m_magicLevel{ 0 };
    uint16_t m_magicLevelPercent{ 0 };
    uint16_t m_baseMagicLevel{ 0 };
    uint16_t m_magicLevelLoyalty{ 0 };
    uint16_t m_magicLevelPercentHundredths{ 0 };
    std::vector<uint16_t> m_spells;
    std::array<SkillSnapshot, tibia::client::LastSkill> m_skills{};
    std::map<tibia::client::ExperienceRate_t, uint16_t> m_experienceRates;
    uint16_t m_speed{ 0 };
    uint16_t m_baseSpeed{ 0 };
    uint16_t m_regenerationTime{ 0 };
    uint16_t m_offlineTrainingTime{ 0 };
    uint16_t m_storeExpBoostTime{ 0 };
    uint16_t m_dailyRewardExpBoostTime{ 0 };
    uint8_t m_rottenBloodState{ 0 };
    std::map<tibia::client::ResourceTypes_t, uint64_t> m_resourcesBalance;
};

}
}
