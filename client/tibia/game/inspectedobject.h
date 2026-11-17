#pragma once

// The inspected objects of InspectionList and of the character info's inspection page.

#include "container/containerciptypes.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/game/thingsession_forward.h>

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
    TAppearanceType* type = tibia::game::rawThingType(slot.typeId, ThingCategoryItem);
    dest.hookSouth = type && type->isHookSouth();
    dest.hookEast = type && type->isHookEast();
    dest.hasProficiency = type && type->getProficiencyId() != 0;
}

}
