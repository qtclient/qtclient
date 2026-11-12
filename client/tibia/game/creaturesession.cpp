#include "creaturesession.h"

#include <client/position.h>
#include "protocol/protocolcodes.h"
#include <creatures/creaturestorage.h>

namespace tibia::game {
namespace {

CreatureOutfitInfo peelOutfit(const Outfit& outfit)
{
    CreatureOutfitInfo info;
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
    return info;
}

CreatureSnapshot peelCreature(const tibia::creatures::TCreature& creature)
{
    CreatureSnapshot snap;
    snap.id = creature.creatureId();
    snap.name = creature.name().toStdString();
    const Position& pos = creature.position();
    snap.pos = { pos.x, pos.y, pos.z };
    snap.healthPercent = creature.healthPercent();
    snap.shield = creature.shield();
    snap.vocation = creature.vocation();
    snap.creatureType = creature.creatureType();
    snap.isPlayer = creature.isPlayer();
    snap.isNpc = creature.isNpc();
    snap.canBeSeen = creature.canBeSeen();
    snap.isDead = creature.isDead();
    snap.isLocalPlayer = creature.isLocalPlayer();
    snap.outfit = peelOutfit(creature.outfit());
    return snap;
}

Position toPosition(const MapPos& p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

} // namespace

std::vector<CreatureSnapshot> spectatorsNear(const MapPos center, const bool multiFloor, const int left,
                                             const int right, const int top, const int bottom)
{
    std::vector<CreatureSnapshot> out;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return out;
    for (const auto& creature :
         storage->spectatorsNear(toPosition(center), multiFloor, left, right, top, bottom)) {
        if (creature)
            out.push_back(peelCreature(*creature));
    }
    return out;
}

CreatureSnapshot creatureAt(const uint32_t id)
{
    if (id == 0)
        return {};
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return {};
    const auto creature = storage->creatureAt(id);
    if (!creature)
        return {};
    return peelCreature(*creature);
}

bool creatureTypeIsSummon(const uint8_t creatureType)
{
    return creatureType == Proto::CreatureTypeSummonOwn
        || creatureType == Proto::CreatureTypeSummonOther;
}

} // namespace tibia::game
