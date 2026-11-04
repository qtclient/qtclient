#include "minimapworldfeed.h"

#include <tibia/assets/thingtype.h>
#include <tibia/world/world.h>

#include "appearances/appearanceinstance.h"

#include <algorithm>
#include <ranges>
#include "worldmap/worldmapstorage.h"

namespace tibia::minimap {

namespace {

constexpr int MaxCost = 250;

}

TMinimapWorldFeed::TMinimapWorldFeed(TMinimapTileStorage& storage)
    : m_storage(storage)
{
    m_storage.setWorld(this);
}

TMinimapWorldFeed::~TMinimapWorldFeed()
{
    m_storage.setWorld(nullptr);
}

bool TMinimapWorldFeed::isKnown(const Position& coord) const
{
    tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(coord);
    return tile && !tile->isEmpty();
}

// CIP: the field's minimap data (FUN_101053680), objects in field order.
TMinimapFieldData TMinimapWorldFeed::fieldData(const Position& coord) const
{
    TMinimapFieldData data;
    tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(coord);
    if (!tile)
        return data;
    const auto& appearances = tile->appearances();

    // The topmost object with an automap colour (FUN_10105e690).
    data.color = 0;
    for (const auto& appearance : std::views::reverse(appearances)) {
        const ThingType* type = appearance ? appearance->thingType() : nullptr;
        if (type && type->hasMiniMapColor()) {
            data.color = static_cast<uint8_t>(type->getMinimapColor());
            break;
        }
    }

    // The first object with a bank (FUN_10105e750), 0xff without one; at most 250.
    int cost = 0xff;
    for (const auto& appearance : appearances) {
        const ThingType* type = appearance ? appearance->thingType() : nullptr;
        if (type && type->isGround()) {
            cost = type->getGroundSpeed();
            break;
        }
    }
    data.cost = static_cast<uint8_t>(std::min(cost, MaxCost));

    // Any unpassable object that is not a creature (FUN_10105d1a0).
    data.blocked = std::ranges::any_of(appearances, [](const appearances::TAppearanceInstancePtr& appearance) {
        const ThingType* type = appearance ? appearance->thingType() : nullptr;
        return type && type->getCategory() != ThingCategoryCreature && type->isNotWalkable();
    });
    return data;
}

void TMinimapWorldFeed::forEachKnownField(const int z, const std::function<void(const Position&)>& visit) const
{
    for (tibia::worldmap::TWorldMapField* tile : tibia::world::fields().getTiles(static_cast<int8_t>(z))) {
        if (!tile->isEmpty())
            visit(tile->getPosition());
    }
}

// CIP: the world area shifted one field per floor of perspective (FUN_10107f2c0).
QRect TMinimapWorldFeed::areaOn(const int z) const
{
    const Position center = tibia::world::fields().getCentralPosition();
    const AwareRange range = tibia::world::fields().getAwareRange();
    const int shift = center.z - z;
    return QRect(center.x - range.left + shift, center.y - range.top + shift, range.horizontal(),
                 range.vertical());
}

}
