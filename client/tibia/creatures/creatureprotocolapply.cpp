#include "creatures/creatureprotocolapply.h"

#include "creatures/creaturestorage.h"
#include "game/localplayersessionthing.h"
#include "protocol/protocolcodes.h"
#include "worldmap/worldmapstorage.h"

#include <client/outfit.h>
#include <framework/util/color.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>


namespace tibia::creatures {

namespace {

constexpr uint32_t NewCreature = 0x61;
constexpr uint32_t TurnedCreature = 0x63;

}

Outfit outfitFromProtocol(const protobuf::protocol::ProtocolOutfit& look, const protobuf::protocol::ProtocolOutfit* mount)
{
    Outfit outfit;
    if (uint16_t lookType = static_cast<uint16_t>(look.look_type()); lookType != 0) {
        outfit.setCategory(ThingCategoryCreature);
        if (!game::isValidDatId(lookType, ThingCategoryCreature))
            lookType = 0;
        outfit.setId(lookType);
        outfit.setHead(static_cast<uint8_t>(look.head()));
        outfit.setBody(static_cast<uint8_t>(look.body()));
        outfit.setLegs(static_cast<uint8_t>(look.legs()));
        outfit.setFeet(static_cast<uint8_t>(look.feet()));
        outfit.setAddons(static_cast<uint8_t>(look.addons()));
    } else if (uint16_t item = static_cast<uint16_t>(look.look_type_ex()); item == 0) {
        outfit.setCategory(ThingCategoryEffect);
        outfit.setAuxId(13);
    } else {
        if (!game::isValidDatId(item, ThingCategoryItem))
            item = 0;
        outfit.setCategory(ThingCategoryItem);
        outfit.setAuxId(item);
    }
    if (mount) {
        if (mount->look_type() != 0)
            outfit.setMountColors(static_cast<uint8_t>(mount->head()), static_cast<uint8_t>(mount->body()),
                                  static_cast<uint8_t>(mount->legs()), static_cast<uint8_t>(mount->feet()));
        outfit.setMount(static_cast<uint16_t>(mount->look_type()));
    }
    return outfit;
}

uint32_t applyProtocolCreature(const protobuf::protocol::ProtocolCreature& c)
{
    const uint32_t id = c.id();
    const LocalPlayerSessionThingPtr player = game::localPlayer();
    if (c.kind() == TurnedCreature) {
        protocolSetDirection(id, static_cast<uint8_t>(c.direction()));
        if (player && id == player->getId())
            player->turn(static_cast<client::Direction>(c.direction()));
        protocolSetPassable(id, !c.unpassable());
        return id;
    }

    const bool known = c.kind() != NewCreature;
    uint8_t type = static_cast<uint8_t>(c.type());
    uint32_t masterId = c.master_id();
    if (type == Proto::CreatureTypeSummonOwn && player && player->getId() != masterId)
        type = Proto::CreatureTypeSummonOther;
    bool isLocalPlayer = false;
    if (known) {
        ensureFromProtocol(id, {});
    } else {
        if (c.remove_id() != id) {
            worldmap::TWorldMapStorage::ensureStorage()->removeCreatureFromFields(c.remove_id());
            if (TCreatureStorage* storage = TCreatureStorage::instance())
                storage->remove(c.remove_id());
        }
        const std::string name = game::formatCreatureName(c.name());
        ensureFromProtocol(id, QString::fromStdString(name), {}, 2, type, type == Proto::CreatureTypePlayer,
                           type == Proto::CreatureTypeNpc);
        isLocalPlayer = player
            && (id == player->getId() || (type == Proto::CreatureTypePlayer && !player->getId() && name == player->getName()));
        if (isLocalPlayer) {
            player->setId(id);
            player->setName(name);
            player->setMasterId(masterId);
            player->setType(type);
        }
    }

    Outfit outfit = outfitFromProtocol(c.outfit(), &c.mount());
    std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> icons;
    for (const auto& icon : c.icons())
        icons.emplace_back(static_cast<uint8_t>(icon.icon()), static_cast<uint8_t>(icon.category()),
                           static_cast<uint16_t>(icon.value()));
    protocolSetIcons(id, icons);

    // The creature's own type (the second type byte, sent for every creature).
    uint8_t creatureType = static_cast<uint8_t>(c.type());
    if (creatureType == Proto::CreatureTypeSummonOwn) {
        masterId = c.master_id();
        if (player && player->getId() != masterId)
            creatureType = Proto::CreatureTypeSummonOther;
    } else if (creatureType == Proto::CreatureTypePlayer) {
        protocolSetVocation(id, static_cast<uint8_t>(c.vocation()));
    }
    if (c.mark() == 0xff)
        protocolHideStaticSquare(id);
    else
        protocolShowStaticSquare(id, Color::from8bit(static_cast<int>(c.mark())));
    protocolSetInspectionType(id, static_cast<uint8_t>(c.inspection_type()));

    const auto direction = static_cast<uint8_t>(c.direction());
    ensureFromProtocol(id, {}, outfit, direction);
    protocolSetHealthPercent(id, static_cast<uint8_t>(c.health_percent()));
    protocolSetDirection(id, direction);
    protocolSetOutfit(id, outfit);
    protocolSetSpeed(id, static_cast<uint16_t>(c.speed()), 0);
    protocolSetSkull(id, static_cast<uint8_t>(c.skull()));
    protocolSetShield(id, static_cast<uint8_t>(c.shield()));
    protocolSetPassable(id, !c.unpassable());
    Light light;
    light.intensity = static_cast<uint8_t>(c.light_intensity());
    light.color = static_cast<uint8_t>(c.light_color());
    protocolSetLight(id, light);
    if (c.emblem() > 0) {
        if (TCreatureStorage* storage = TCreatureStorage::instance()) {
            const auto row = storage->getOrCreate(id);
            row->setEmblem(static_cast<uint8_t>(c.emblem()));
            storage->publishCreatureUpdated(row);
        }
    }
    ensureFromProtocol(id, {}, {}, direction, creatureType, creatureType == Proto::CreatureTypePlayer,
                       creatureType == Proto::CreatureTypeNpc);
    if (c.icon() > 0)
        protocolSetIcon(id, static_cast<uint8_t>(c.icon()));


    if (isLocalPlayer) {
        player->turn(static_cast<client::Direction>(direction));
        player->setOutfit(outfit, true, false);
        player->setMasterId(masterId);
        if (!player->isKnown())
            game::localPlayerSetKnown(true);
    }
    return id;
}

}
