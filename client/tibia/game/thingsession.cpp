#include "thingsession.h"

#include <tibia/assets/things.h>
#include <tibia/creatures/attachedeffect.h>
#include <tibia/creatures/attachedeffectmanager.h>
#include <tibia/creatures/creaturestorage.h>

#include <algorithm>

namespace tibia::game {

std::string itemName(const uint16_t id)
{
    if (!isValidItemId(id))
        return {};
    if (const ThingTypePtr& type = g_things.getThingType(id, ThingCategoryItem); type)
        return type->getName();
    return {};
}

bool isValidItemId(const uint16_t id)
{
    return g_things.isValidDatId(id, ThingCategoryItem);
}

bool setItemThingShape(const uint16_t id, const std::string_view shapeName)
{
    return g_things.setThingShape(id, std::string(shapeName));
}

bool itemIsContainer(const uint16_t id)
{
    if (!isValidItemId(id))
        return false;
    const ThingTypePtr& type = g_things.getThingType(id, ThingCategoryItem);
    return type && type->isContainer();
}

bool itemIsPickupable(const uint16_t id)
{
    if (!isValidItemId(id))
        return false;
    const ThingTypePtr& type = g_things.getThingType(id, ThingCategoryItem);
    return type && type->isPickupable();
}

int64_t itemMeanPrice(const uint16_t id)
{
    if (!isValidItemId(id))
        return 0;
    const ThingTypePtr& type = g_things.getThingType(id, ThingCategoryItem);
    return type ? static_cast<int64_t>(type->getMeanPrice()) : 0;
}

int64_t itemMaxNpcSaleGoldPrice(const uint16_t id)
{
    if (!isValidItemId(id))
        return 0;
    const ThingTypePtr& type = g_things.getThingType(id, ThingCategoryItem);
    if (!type)
        return 0;
    int64_t npcSale = 0;
    for (const NPCData& npc : type->getNpcSaleData()) {
        if (npc.currencyObjectTypeId == 0 && npc.currencyQuestFlagDisplayName.empty())
            npcSale = std::max<int64_t>(npcSale, static_cast<int64_t>(npc.salePrice));
    }
    return npcSale;
}

std::string raceName(const uint32_t raceId)
{
    const RaceType& race = g_things.getRaceData(raceId);
    return race.raceId != 0 ? race.name : std::string{};
}

bool raceExists(const uint32_t raceId)
{
    return g_things.getRaceData(raceId).raceId != 0;
}

bool raceIsBoss(const uint32_t raceId)
{
    const RaceType& race = g_things.getRaceData(raceId);
    return race.raceId != 0 && race.boss;
}

std::vector<uint32_t> raceIdsByName(const std::string& search)
{
    std::vector<uint32_t> out;
    for (const RaceType& race : g_things.getRacesByName(search)) {
        if (race.raceId != 0)
            out.push_back(race.raceId);
    }
    return out;
}

std::vector<uint32_t> allRaceIds()
{
    std::vector<uint32_t> out;
    for (const RaceType& race : g_things.getAllRaces()) {
        if (race.raceId != 0)
            out.push_back(race.raceId);
    }
    return out;
}

RaceOutfitInfo raceOutfit(const uint32_t raceId)
{
    RaceOutfitInfo info;
    const RaceType& race = g_things.getRaceData(raceId);
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
    for (const CyclopediaQuestType& quest : g_things.getCyclopediaQuests()) {
        if (quest.id == 0)
            continue;
        out.push_back({ true, quest.id, quest.name });
    }
    return out;
}

QuestInfo questInfo(const uint16_t id)
{
    QuestInfo info;
    if (const CyclopediaQuestType* quest = g_things.getCyclopediaQuest(id)) {
        info.exists = true;
        info.id = quest->id;
        info.name = quest->name;
    }
    return info;
}

void clearEffectVariants()
{
    g_things.clearEffectVariants();
}

void registerEffectVariant(const uint16_t virtualId, const uint16_t baseId, const std::string& shader,
                           const bool underCreatures)
{
    g_things.registerEffectVariant(virtualId, baseId, shader, underCreatures);
}

void removeAttachedEffect(const uint16_t id)
{
    g_attachedEffects.remove(id);
}

void registerAttachedEffectByThing(const uint16_t id, const std::string_view note, const uint16_t baseId,
                                   const uint8_t category)
{
    if (const AttachedEffectPtr& effect = g_attachedEffects.registerByThing(
            id, note, baseId, static_cast<ThingCategory>(category))) {
        effect->setPermanent(false);
        effect->setOnTop(false);
    }
}

void attachCreatureEffect(const uint32_t creatureId, const uint16_t effectId)
{
    const AttachedEffectPtr& effect = g_attachedEffects.getById(effectId);
    if (!effect)
        return;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(creatureId)) {
            for (const auto& attached : row->attachedEffects()) {
                if (attached && attached->getId() == effectId)
                    return;
            }
        }
    }
    const auto clone = effect->clone();
    clone->setPermanent(false);
    tibia::creatures::protocolAttachEffect(creatureId, clone);
}

void detachCreatureEffect(const uint32_t creatureId, const uint16_t effectId)
{
    tibia::creatures::protocolDetachEffect(creatureId, effectId);
}

} // namespace tibia::game
