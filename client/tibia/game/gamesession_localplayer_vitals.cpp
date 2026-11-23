#include "gamesession.h"
#include "game/currentgamesession.h"

#include "game/gameciptypes.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"

#include <tibia/creatures/creaturestorage.h>
#include "game/gamesessionstate.h"

namespace tibia::game {
namespace {

template<typename WriteData>
void playerDataAuthorityWrite(WriteData&& writeData)
{
    if (TPlayerData* data = currentPlayerData())
        writeData(*data);
}

} // namespace

void localPlayerSetHealth(const uint32_t health, const uint32_t maxHealth)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateHealth(health, maxHealth); });
}

void localPlayerSetMana(const uint32_t mana, const uint32_t maxMana)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateMana(mana, maxMana); });
}

void localPlayerSetManaShield(const uint32_t manaShield, const uint32_t maxManaShield)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateManaShield(manaShield, maxManaShield); });
}

void localPlayerSetLevel(const uint16_t level, const uint16_t levelPercent)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateLevel(level, levelPercent); });
}

void localPlayerSetExperience(const uint64_t experience)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateExperience(experience); });
}

void localPlayerSetStamina(const uint16_t stamina)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateStamina(stamina); });
}

void localPlayerSetSoul(const uint8_t soul)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateSoul(soul); });
}

void localPlayerSetFreeCapacity(const uint32_t hundredths)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateFreeCapacity(hundredths); });
}

void localPlayerSetBlessings(const uint16_t blessings, const uint8_t blessVisualState)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateBlessings(blessings, blessVisualState); });
}

void localPlayerSetResourceBalance(const tibia::client::ResourceTypes_t type, const uint64_t value)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setResourceBalance(type, value); });
}

void localPlayerSetHarmony(const uint8_t harmony)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateHarmony(harmony); });
}

void localPlayerSetSerene(const bool serene)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateSerene(serene); });
}

void localPlayerSetExperienceRate(const tibia::client::ExperienceRate_t type, const uint16_t value)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setExperienceRate(type, value); });
}

void localPlayerSetMagicLevel(const uint16_t magicLevel, const uint16_t magicLevelPercent)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setMagicLevel(magicLevel, magicLevelPercent); });
}

void localPlayerSetBaseMagicLevel(const uint16_t baseMagicLevel)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setBaseMagicLevel(baseMagicLevel); });
}

void localPlayerSetMagicLevelDetail(const uint16_t loyaltyLevel, const uint16_t percentHundredths)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setMagicLevelDetail(loyaltyLevel, percentHundredths); });
}

void localPlayerSetSkill(const tibia::client::Skill skill, const uint16_t level, const uint16_t levelPercent)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setSkillLevel(skill, level, levelPercent); });
}

void localPlayerSetSkillDetail(const tibia::client::Skill skill, const uint16_t loyaltyLevel,
                               const uint16_t percentHundredths)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setSkillDetail(skill, loyaltyLevel, percentHundredths); });
}

void localPlayerSetBaseSkill(const tibia::client::Skill skill, const uint16_t baseLevel)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setSkillBaseLevel(skill, baseLevel); });
}

void localPlayerSetBaseSpeed(const uint16_t baseSpeed)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateBaseSpeed(baseSpeed); });
}

void localPlayerSetRegenerationTime(const uint16_t regenerationTime)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setRegenerationTime(regenerationTime); });
}

void localPlayerSetOfflineTrainingTime(const uint16_t offlineTrainingTime)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setOfflineTrainingTime(offlineTrainingTime); });
}

void localPlayerSetStoreExpBoostTime(const uint16_t storeExpBoostTime)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setStoreExpBoostTime(storeExpBoostTime); });
}

void localPlayerSetDailyRewardExpBoostTime(const uint16_t dailyRewardExpBoostTime)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setDailyRewardExpBoostTime(dailyRewardExpBoostTime); });
}

void localPlayerSetStoreExpBoostPurchasable(const bool purchasable)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateStoreExpBoostPurchasable(purchasable); });
}

void localPlayerSetStatesAndRottenBlood(const uint64_t states, const uint8_t rottenBloodState)
{
    (void)states;
    playerDataAuthorityWrite([&](TPlayerData& data) { data.setRottenBloodState(rottenBloodState); });
}

void localPlayerSetTotalCapacity(const uint32_t totalCapacity)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateTotalCapacity(totalCapacity); });
}

void localPlayerSetBaseCapacity(const uint32_t baseCapacity)
{
    playerDataAuthorityWrite([&](TPlayerData& data) { data.updateBaseCapacity(baseCapacity); });
}

uint32_t localPlayerManaShield()
{
    if (TPlayerData* data = currentPlayerData())
        return data->manaShield();
    return 0;
}

uint32_t localPlayerMaxManaShield()
{
    if (TPlayerData* data = currentPlayerData())
        return data->maxManaShield();
    return 0;
}

uint64_t localPlayerExperience()
{
    if (TPlayerData* data = currentPlayerData())
        return data->experience();
    return 0;
}

bool localPlayerIsPremium()
{
    if (TPremiumTimeStorage* storage = TPremiumTimeStorage::liveInstance())
        return storage->isPremium();
    return false;
}

bool localPlayerIsMagicShieldVocation()
{
    if (TPlayerData* data = currentPlayerData())
        return data->magicShieldVocation();
    return false;
}

uint8_t localPlayerHarmony()
{
    if (TPlayerData* data = currentPlayerData())
        return data->harmony();
    return 0;
}

bool localPlayerIsSerene()
{
    if (TPlayerData* data = currentPlayerData())
        return data->serene();
    return false;
}

uint64_t localPlayerResourceBalance(const tibia::client::ResourceTypes_t type)
{
    if (TPlayerData* data = currentPlayerData())
        return data->resourceBalance(type);
    return 0;
}

bool localPlayerHasResourceBalance(const tibia::client::ResourceTypes_t type)
{
    if (TPlayerData* data = currentPlayerData())
        return data->hasResourceBalance(type);
    return false;
}

uint16_t localPlayerSkillLevel(const tibia::client::Skill skill)
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    if (TPlayerData* data = currentPlayerData())
        return data->skillLevel(skill);
    return 0;
}

uint16_t localPlayerSkillBaseLevel(const tibia::client::Skill skill)
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    if (TPlayerData* data = currentPlayerData())
        return data->skillBaseLevel(skill);
    return 0;
}

uint16_t localPlayerSkillLoyaltyLevel(const tibia::client::Skill skill)
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    if (TPlayerData* data = currentPlayerData())
        return data->skillLoyaltyLevel(skill);
    return 0;
}

uint16_t localPlayerSkillPercentHundredths(const tibia::client::Skill skill)
{
    if (skill >= tibia::client::LastSkill)
        return 0;
    if (TPlayerData* data = currentPlayerData())
        return data->skillPercentHundredths(skill);
    return 0;
}

uint16_t localPlayerMagicLevelLoyalty()
{
    if (TPlayerData* data = currentPlayerData())
        return data->magicLevelLoyalty();
    return 0;
}

uint16_t localPlayerMagicLevelPercentHundredths()
{
    if (TPlayerData* data = currentPlayerData())
        return data->magicLevelPercentHundredths();
    return 0;
}

uint16_t localPlayerLevelPercentHundredths()
{
    if (TPlayerData* data = currentPlayerData())
        return data->levelPercentRaw();
    return 0;
}

uint16_t localPlayerExperienceRate(const tibia::client::ExperienceRate_t type)
{
    if (TPlayerData* data = currentPlayerData())
        return data->experienceRate(type);
    return 0;
}

uint16_t localPlayerSpeed()
{
    if (TPlayerData* data = currentPlayerData())
        return data->speed();
    return 0;
}

uint16_t localPlayerBaseSpeed()
{
    if (TPlayerData* data = currentPlayerData())
        return data->baseSpeed();
    return 0;
}

uint16_t localPlayerRegenerationTime()
{
    if (TPlayerData* data = currentPlayerData())
        return data->regenerationTime();
    return 0;
}

uint16_t localPlayerOfflineTrainingTime()
{
    if (TPlayerData* data = currentPlayerData())
        return data->offlineTrainingTime();
    return 0;
}

uint16_t localPlayerStoreExpBoostTime()
{
    if (TPlayerData* data = currentPlayerData())
        return data->storeExpBoostTime();
    return 0;
}

uint16_t localPlayerDailyRewardExpBoostTime()
{
    if (TPlayerData* data = currentPlayerData())
        return data->dailyRewardExpBoostTime();
    return 0;
}

uint8_t localPlayerRottenBloodState()
{
    if (TPlayerData* data = currentPlayerData())
        return data->rottenBloodState();
    return 0;
}

uint64_t localPlayerStates()
{
    if (TPlayerData* data = currentPlayerData())
        return data->states().flags.to_ullong();
    return 0;
}

uint8_t localPlayerSoul()
{
    if (TPlayerData* data = currentPlayerData())
        return data->soul();
    return 0;
}

uint16_t localPlayerMagicLevel()
{
    if (TPlayerData* data = currentPlayerData())
        return data->magicLevel();
    return 0;
}

uint16_t localPlayerBaseMagicLevel()
{
    if (TPlayerData* data = currentPlayerData())
        return data->baseMagicLevel();
    return 0;
}

uint16_t localPlayerMagicLevelPercent()
{
    if (TPlayerData* data = currentPlayerData())
        return data->magicLevelPercent();
    return 0;
}

std::vector<uint16_t> localPlayerSpells()
{
    if (TPlayerData* data = currentPlayerData())
        return data->spells();
    return {};
}

tibia::client::Direction localPlayerDirection()
{
    if (const auto row = localPlayerCreature())
        return static_cast<tibia::client::Direction>(row->patternX());
    return tibia::client::InvalidDirection;
}

bool localPlayerIsSoftDead()
{
    if (const auto row = localPlayerCreature())
        return row->isSoftDead();
    return false;
}

uint8_t localPlayerSkull()
{
    if (const auto row = localPlayerCreature())
        return row->skull();
    return 0;
}

uint16_t localPlayerBlessings()
{
    if (TPlayerData* data = currentPlayerData())
        return data->blessingMask();
    return 0;
}

uint32_t localPlayerBaseCapacity()
{
    if (TPlayerData* data = currentPlayerData())
        return data->baseCapacity();
    return 0;
}

uint32_t localPlayerTotalCapacity()
{
    if (TPlayerData* data = currentPlayerData())
        return data->totalCapacity();
    return 0;
}

bool localPlayerHasEquippedItemId(const uint16_t itemId, const uint8_t tier)
{
    tibia::container::TInventoryContainer* inventory = currentInventoryContainer();
    if (!hasLocalPlayer() || !inventory)
        return false;
    for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
        const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
        if (!row.empty() && row.typeId == itemId && row.tier == tier)
            return true;
    }
    return false;
}

bool localPlayerIsStoreExpBoostPurchasable()
{
    if (TPlayerData* data = currentPlayerData())
        return data->storeExpBoostPurchasable();
    return false;
}

}
