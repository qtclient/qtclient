#pragma once

// The inspected objects of InspectionList and of the character info's inspection page.

#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "game/currentgamesession.h"
#include "gamewindow/inspectobjectdata.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <tibia/appearances/appearanceinstance.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <protocol.pb.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace tibia::game {

inline std::vector<std::pair<std::string, std::string>> inspectionDetails(
    const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolInspectionDetail>& list)
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& d : list)
        out.emplace_back(d.label(), d.value());
    return out;
}

/// The appearance of an inspected object, as the dialogs show it.
template<typename Dest>
void fillInspectedAppearance(Dest& dest, const ::tibia::protobuf::protocol::ProtocolInventoryItem& item)
{
    const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(item);
    dest.typeId = slot.typeId;
    dest.count = slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
    dest.tier = slot.tier;
    dest.fluidOrSubtype = slot.fluidOrSplash ? slot.fluid : 0;
    dest.fluidOrSplash = slot.fluidOrSplash;
    TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, slot.typeId);
    dest.hookSouth = type && type->isHookSouth();
    dest.hookEast = type && type->isHookEast();
    dest.hasProficiency = type && type->getProficiencyId() != 0;
}

/// The dialog data of an inspection list entry, read from its appearance
/// instance (qtclient's inspect dialogs still take this form).
template<typename Dest>
void fillInspectedAppearance(Dest& dest, const ::tibia::gamewindow::TInspectObjectData& entry)
{
    using namespace ::tibia::appearances;
    dest.name = entry.name.toStdString();
    for (const auto& [label, value] : entry.details)
        dest.descriptions.emplace_back(label.toStdString(), value.toStdString());
    dest.imbuements.assign(entry.imbuementSlots.begin(), entry.imbuementSlots.end());
    const TAppearanceInstance* instance = entry.appearance.get();
    if (!instance || instance->isCreature())
        return;
    TAppearanceType* type = instance->thingType();
    dest.typeId = instance->id();
    const auto* object = dynamic_cast<const TObjectAppearanceInstance*>(instance);
    if (dynamic_cast<const TCumulativeObjectAppearanceInstance*>(instance))
        dest.count = std::max<uint16_t>(1, object->count());
    if (object)
        dest.tier = object->tier();
    if (const auto* liquid = dynamic_cast<const TLiquidObjectAppearanceInstance*>(instance)) {
        dest.fluidOrSubtype = static_cast<uint8_t>(liquid->fluid());
        dest.fluidOrSplash = true;
    }
    dest.hookSouth = type && type->isHookSouth();
    dest.hookEast = type && type->isHookEast();
    dest.hasProficiency = type && type->getProficiencyId() != 0;
}

/// The item dialog data of an object inspection list's first entry.
inline ItemInspectionData toItemInspectionData(const std::vector<::tibia::gamewindow::TInspectObjectData>& list,
                                               const int clientWindow)
{
    ItemInspectionData data{};
    data.inspectionType = static_cast<uint8_t>(clientWindow);
    data.creatureId = 0;
    if (!list.empty())
        fillInspectedAppearance(data, list.front());
    return data;
}

/// The player dialog data of a player's inspection list: the entries with an
/// inventory slot are the equipment, the last entry is the player. Without an
/// outfit in the list, the player's creature shows its current one.
inline CyclopediaCharacterInspection toCharacterInspection(const std::vector<::tibia::gamewindow::TInspectObjectData>& list,
                                                           const uint32_t playerId)
{
    CyclopediaCharacterInspection data{};
    data.inspectionType = tibia::client::INSPECT_CYCLOPEDIA;
    data.creatureId = playerId;
    for (const auto& entry : list) {
        if (entry.inventorySlot == ::tibia::gamewindow::TInspectObjectData::NoInventorySlot) {
            data.playerName = entry.name.toStdString();
            for (const auto& [label, value] : entry.details)
                data.playerDescriptions.emplace_back(label.toStdString(), value.toStdString());
            if (const auto* outfit = dynamic_cast<const ::tibia::appearances::TOutfitAppearanceInstance*>(entry.appearance.get()))
                data.outfit = outfit->outfit();
            continue;
        }
        InspectionInventoryItem item{};
        item.slot = static_cast<uint8_t>(entry.inventorySlot);
        fillInspectedAppearance(item, entry);
        data.inventoryItems.push_back(std::move(item));
    }
    if (data.outfit.getId() == 0) {
        if (auto* storage = ::tibia::game::currentCreatureStorage()) {
            if (const auto row = storage->creatureAt(playerId))
                data.outfit = row->outfit();
        }
    }
    return data;
}

}
