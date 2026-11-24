#include "thingsession.h"
#include "game/currentgamesession.h"
#include "cyclopedia/staticcontentinformation.h"

#include <tibia/assets/things.h>
#include <tibia/creatures/creaturestorage.h>

#include <algorithm>

namespace tibia::game {

std::string itemName(const uint16_t id)
{
    if (!isValidItemId(id))
        return {};
    if (const TAppearanceTypePtr& type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id); type)
        return type->getName();
    return {};
}

bool isValidItemId(const uint16_t id)
{
    return tibia::appearances::TAppearancesManager::instance().isValidDatId(id, ThingCategoryItem);
}

bool itemIsContainer(const uint16_t id)
{
    if (!isValidItemId(id))
        return false;
    const TAppearanceTypePtr& type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id);
    return type && type->isContainer();
}

bool itemIsPickupable(const uint16_t id)
{
    if (!isValidItemId(id))
        return false;
    const TAppearanceTypePtr& type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id);
    return type && type->isPickupable();
}

int64_t itemMeanPrice(const uint16_t id)
{
    if (!isValidItemId(id))
        return 0;
    const TAppearanceTypePtr& type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id);
    return type ? static_cast<int64_t>(type->getMeanPrice()) : 0;
}

int64_t itemMaxNpcSaleGoldPrice(const uint16_t id)
{
    if (!isValidItemId(id))
        return 0;
    const TAppearanceTypePtr& type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, id);
    if (!type)
        return 0;
    int64_t npcSale = 0;
    for (const auto& npc : type->npcTradeData()) {
        if (!npc.currency)
            npcSale = std::max<int64_t>(npcSale, static_cast<int64_t>(npc.salePrice));
    }
    return npcSale;
}

std::string raceName(const uint32_t raceId)
{
    const RaceType& race = tibia::appearances::TAppearancesManager::instance().getRaceData(raceId);
    return race.raceId != 0 ? race.name : std::string{};
}

bool raceExists(const uint32_t raceId)
{
    return tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).raceId != 0;
}

bool raceIsBoss(const uint32_t raceId)
{
    const RaceType& race = tibia::appearances::TAppearancesManager::instance().getRaceData(raceId);
    return race.raceId != 0 && race.boss;
}

std::vector<uint32_t> raceIdsByName(const std::string& search)
{
    std::vector<uint32_t> out;
    for (const RaceType& race : tibia::appearances::TAppearancesManager::instance().getRacesByName(search)) {
        if (race.raceId != 0)
            out.push_back(race.raceId);
    }
    return out;
}

std::vector<uint32_t> allRaceIds()
{
    std::vector<uint32_t> out;
    for (const RaceType& race : tibia::appearances::TAppearancesManager::instance().getAllRaces()) {
        if (race.raceId != 0)
            out.push_back(race.raceId);
    }
    return out;
}

RaceOutfitInfo raceOutfit(const uint32_t raceId)
{
    RaceOutfitInfo info;
    const RaceType& race = tibia::appearances::TAppearancesManager::instance().getRaceData(raceId);
    if (race.raceId == 0)
        return info;
    const Outfit& outfit = race.outfit;
    const bool isObject = outfit.getId() == 0 && outfit.getAuxId() != 0;
    const uint16_t looktype = isObject ? outfit.getAuxId() : outfit.getId();
    if (looktype == 0)
        return info;
    info.exists = true;
    info.isObject = isObject;
    info.looktype = looktype;
    info.head = outfit.getHead();
    info.body = outfit.getBody();
    info.legs = outfit.getLegs();
    info.feet = outfit.getFeet();
    info.addons = outfit.getAddons();
    info.name = race.name;
    return info;
}

std::vector<QuestInfo> allQuestInfos()
{
    std::vector<QuestInfo> out;
    for (const cyclopedia::TStaticQuest& quest : cyclopedia::staticQuests()) {
        if (quest.id == 0)
            continue;
        out.push_back({ true, static_cast<uint16_t>(quest.id), quest.name.toStdString() });
    }
    return out;
}

QuestInfo questInfo(const uint16_t id)
{
    QuestInfo info;
    if (cyclopedia::hasQuest(id)) {
        info.exists = true;
        info.id = id;
        info.name = cyclopedia::questName(id).toStdString();
    }
    return info;
}

} // namespace tibia::game
