#include "creatures/creatureprotocolapply.h"

#include "creatures/creaturestorage.h"
#include "game/localplayersessionthing.h"
#include "protocol/protocolcodes.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <client/outfit.h>
#include <framework/util/color.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

#include <QDebug>

#include <optional>
#include <set>

namespace tibia::creatures {

namespace {

constexpr uint32_t NewCreature = 0x61;
constexpr uint32_t KnownCreature = 0x62;

/// The NPC category shown for the protocol's creature icon; other values show none.
uint8_t npcCategory(const uint32_t icon)
{
    static constexpr uint8_t Categories[8] = { 0, 1, 2, 0, 0, 3, 0, 4 };
    return icon < 8 ? Categories[icon] : 0;
}

/// A vocation the client knows (0 none, 1-5 the vocations); nothing otherwise.
std::optional<uint8_t> vocationFromProtocol(const uint32_t vocation)
{
    if (vocation < 6)
        return static_cast<uint8_t>(vocation);
    return std::nullopt;
}

/// A player icon the client knows (0..22); nothing otherwise.
std::optional<uint8_t> playerIconFromProtocol(const uint32_t icon)
{
    if (icon <= 22)
        return static_cast<uint8_t>(icon);
    return std::nullopt;
}

/// A creature icon the client knows (0..9); nothing otherwise.
std::optional<uint8_t> creatureIconFromProtocol(const uint32_t icon)
{
    if (icon <= 9)
        return static_cast<uint8_t>(icon);
    return std::nullopt;
}

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

Color colorFromEightBit(const uint8_t color)
{
    if (color > 215)
        return Color(0, 0, 0);
    return Color(color / 36 * 51, color / 6 % 6 * 51, color % 6 * 51);
}

void syncFieldStackFor(const uint32_t creatureId)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    const auto creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature || !creature->position().isValid())
        return;
    if (tibia::worldmap::TWorldMapField* field = tibia::worldmap::fieldAt(creature->position()))
        field->syncCreatureStackAppearance(creatureId);
}

bool applyProtocolCreatureFields(TCreatureStorage& storage, const std::shared_ptr<TCreature>& creature,
                                 const protobuf::protocol::ProtocolCreature& c)
{
    TCreature& target = *creature;
    if (c.has_name())
        target.setName(QString::fromStdString(c.name()));
    if (c.has_health_percent())
        target.setHealthPercent(static_cast<uint8_t>(c.health_percent()));
    if (c.has_mana_percent())
        target.setManaPercent(static_cast<uint8_t>(c.mana_percent()));
    if (c.has_direction())
        target.setDirection(static_cast<uint8_t>(c.direction()));
    if (c.has_light_intensity())
        target.setLight(Light(static_cast<uint8_t>(c.light_intensity()), static_cast<uint8_t>(c.light_color())));
    if (c.has_speed())
        target.setSpeed(static_cast<uint16_t>(c.speed()));
    if (c.has_skull())
        target.setSkull(static_cast<uint8_t>(c.skull()));
    if (c.has_shield())
        target.setShield(static_cast<uint8_t>(c.shield()));
    if (c.has_inspection_type())
        target.setInspectionType(static_cast<uint8_t>(c.inspection_type()));
    if (c.has_emblem())
        target.setEmblem(static_cast<uint8_t>(c.emblem()));
    if (c.has_type())
        target.setCreatureType(static_cast<uint8_t>(c.type()));
    if (c.has_master_id())
        target.setMasterId(c.master_id());
    if (c.has_icon())
        target.setIcon(npcCategory(c.icon()));
    if (c.has_unpassable())
        target.setUnpassable(c.unpassable());
    if (c.has_mark()) {
        if (static_cast<uint8_t>(c.mark()) == 0xff)
            target.hideStaticSquare();
        else
            target.showStaticSquare(colorFromEightBit(static_cast<uint8_t>(c.mark())));
    }
    if (c.has_show_status())
        target.setShowStatus(c.show_status());
    if (c.has_vocation()) {
        // An unknown vocation abandons the rest of the message.
        const std::optional<uint8_t> vocation = vocationFromProtocol(c.vocation());
        if (!vocation) {
            qWarning() << "creature" << c.id() << "carries unknown vocation" << c.vocation();
            return false;
        }
        target.setVocation(*vocation);
    }
    if (c.has_update_icons()) {
        target.clearIcons();
        for (const auto& icon : c.icons()) {
            // A player shows only player icons.
            const uint32_t kind = icon.category();
            if (kind > 1 || (kind == 1 && target.isPlayer()))
                continue;
            // An unknown icon abandons the rest of the message.
            const std::optional<uint8_t> id = kind == 0 ? playerIconFromProtocol(icon.icon())
                                                        : creatureIconFromProtocol(icon.icon());
            if (!id) {
                qWarning() << "creature" << c.id() << "carries unknown icon" << icon.icon() << "of kind" << kind;
                return false;
            }
            target.addIcon({ kind, *id, static_cast<uint16_t>(icon.value()), icon.flag() });
        }
    }
    if (c.update_extra_icons())
        target.setExtraIcons(std::set<uint8_t>(c.extra_icons().begin(), c.extra_icons().end()));
    if (c.has_outfit()) {
        target.setOutfit(outfitFromProtocol(c.outfit(), c.has_mount() ? &c.mount() : nullptr));
        storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Outfit);
    }
    if (c.has_mount()) {
        // A mount without a look type dismounts.
        target.setMounted(c.mount().look_type() != 0);
        storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Mount);
    }
    if (c.has_outfit() || c.has_direction())
        syncFieldStackFor(c.id());
    return true;
}

uint32_t applyProtocolCreature(const protobuf::protocol::ProtocolCreature& c)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return 0;
    const uint32_t id = c.id();
    const LocalPlayerSessionThingPtr player = game::localPlayer();
    // Host: before the login named it, the session takes its own id from the
    // own character's creature.
    if (player && !player->getId() && c.kind() == NewCreature && c.type() == Proto::CreatureTypePlayer
        && c.name() == player->getName()) {
        player->setId(id);
        storage->setPlayerID(id);
    }

    // A new creature replaces the one the message names; known and turned
    // creatures (and a new one naming itself) must already be stored.
    const uint32_t removeId = c.kind() == NewCreature ? c.remove_id() : id;
    const bool created = removeId != id;
    std::shared_ptr<TCreature> creature;
    if (!created) {
        creature = storage->creatureAt(id);
        if (!creature) {
            qWarning() << "updateOrCreateCreatureFromProtobuf: no creature with id" << id << "found";
            return 0;
        }
    } else {
        worldmap::TWorldMapStorage::ensureStorage()->removeCreatureFromFields(removeId);
        storage->remove(removeId);
        creature = storage->createCreature(id);
    }

    if (!applyProtocolCreatureFields(*storage, creature, c))
        return 0;
    if (created) {
        creature->renewUniqueId();
        if (creature->isLocalPlayer())
            storage->notifyPlayerAdded(creature);
    }
    storage->notifyCreatureUpdated(creature);

    // Host: the session's own character follows its creature.
    if (player && id == player->getId()) {
        if (created) {
            player->setName(c.name());
            player->setType(creature->creatureType());
        }
        if (c.has_master_id())
            player->setMasterId(c.master_id());
        if (c.has_direction())
            player->turn(static_cast<client::Direction>(c.direction()));
        if (c.has_outfit())
            player->setOutfit(creature->outfit(), true, false);
        if ((c.kind() == NewCreature || c.kind() == KnownCreature) && !player->isKnown())
            game::localPlayerSetKnown(true);
    }
    return id;
}

}
