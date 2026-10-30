#include "minimapworldfeed.h"

#include "worldmap/worldmapprotocolmessagehandler.h"

#include <tibia/world/world.h>

#include <algorithm>
#include <ranges>

namespace tibia::minimap {

namespace {

constexpr int MaxCost = 250;

}

TMinimapWorldFeed::TMinimapWorldFeed(TMinimapTileStorage& storage)
    : m_storage(storage)
{
    m_storage.setWorld(this);
    tibia::world::fields().setObserver(this);
    // Created after the first map description arrived.
    if (tibia::world::fields().getCentralPosition().isValid())
        onWindowChanged(true);
}

TMinimapWorldFeed::~TMinimapWorldFeed()
{
    tibia::world::fields().setObserver(nullptr);
    m_storage.setWorld(nullptr);
}

void TMinimapWorldFeed::onTileChanged(const Position& pos)
{
    m_storage.onFieldChanged(pos);
    if (m_protocol)
        m_protocol->onTileChanged(pos);
}

void TMinimapWorldFeed::onWindowChanged(const bool fullDescription)
{
    // CIP: after a full map or a floor change (TWorldmapProtocolMessageHandler).
    if (m_protocol)
        m_protocol->onMapWindowChanged(fullDescription);
    if (!fullDescription)
        return;
    m_storage.setPlayerZ(tibia::world::fields().getCentralPosition().z);
    m_storage.markUnseen();
}

bool TMinimapWorldFeed::isKnown(const Position& coord) const
{
    const TilePtr& tile = tibia::world::fields().getTile(coord);
    return tile && !tile->isEmpty();
}

// CIP: the field's minimap data (FUN_101053680), objects in field order.
TMinimapFieldData TMinimapWorldFeed::fieldData(const Position& coord) const
{
    TMinimapFieldData data;
    const TilePtr& tile = tibia::world::fields().getTile(coord);
    if (!tile)
        return data;
    const auto& things = tile->getThings();

    // The topmost object with an automap colour (FUN_10105e690).
    data.color = 0;
    for (const ThingPtr& thing : std::views::reverse(things)) {
        if (thing->hasMiniMapColor()) {
            data.color = static_cast<uint8_t>(thing->getMinimapColor());
            break;
        }
    }

    // The first object with a bank (FUN_10105e750), 0xff without one; at most 250.
    int cost = 0xff;
    for (const ThingPtr& thing : things) {
        if (thing->isGround()) {
            cost = thing->getGroundSpeed();
            break;
        }
    }
    data.cost = static_cast<uint8_t>(std::min(cost, MaxCost));

    // Any unpassable object that is not a creature (FUN_10105d1a0).
    data.blocked = std::ranges::any_of(things, [](const ThingPtr& thing) { return !thing->isCreature() && thing->isNotWalkable(); });
    return data;
}

void TMinimapWorldFeed::forEachKnownField(const int z, const std::function<void(const Position&)>& visit) const
{
    for (const TilePtr& tile : tibia::world::fields().getTiles(static_cast<int8_t>(z))) {
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
    return QRect(center.x - range.left + shift, center.y - range.top + shift, range.horizontal(), range.vertical());
}

}
