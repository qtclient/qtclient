#include "protocol/mapwire.h"

#include "worldmap/coordinates.h"

#include <tibia/assets/things.h>
#include <tibia/game/mapsession.h>

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

constexpr uint16_t NewCreature = 0x61;
constexpr uint16_t KnownCreature = 0x62;
constexpr uint16_t TurnedCreature = 0x63;

using worldmap::TShearedWorldMapCuboid;
using worldmap::TWorldMapCentralPosition;
using worldmap::TWorldMapCoordinate;
using worldmap::TWorldMapCuboid;
using worldmap::TWorldMapExtent;

/// The translator's central position (CIP: the translator's member the map
/// messages move) and the one the message being translated moves it to; the
/// move is committed once the message translated completely.
TWorldMapCoordinate g_center;
TWorldMapCoordinate g_pendingCenter;

/// The central position at `center` with the aware area the session uses.
TWorldMapCentralPosition centralPositionAt(const TWorldMapCoordinate& center)
{
    const tibia::game::MapAwareRange range = tibia::game::awareRange();
    return TWorldMapCentralPosition(center, TWorldMapExtent(range.horizontal(), range.vertical(), 1),
                                    TWorldMapExtent(range.left, range.top, 0));
}

void setCoordinate(ProtocolCoordinate* c, const int x, const int y, const int z)
{
    c->set_x(static_cast<uint32_t>(x));
    c->set_y(static_cast<uint32_t>(y));
    c->set_z(static_cast<uint32_t>(z));
}

void setCoordinate(ProtocolCoordinate* c, const TWorldMapCoordinate& coordinate)
{
    setCoordinate(c, coordinate.x, coordinate.y, coordinate.z);
}

bool readPosition(ClassicReader& r, TWorldMapCoordinate& out)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (!r.u16(x) || !r.u16(y) || !r.u8(z))
        return false;
    out = TWorldMapCoordinate(x, y, z);
    return true;
}

bool readBool(ClassicReader& r, bool& out)
{
    uint8_t value = 0;
    if (!r.u8(value))
        return false;
    out = value != 0;
    return true;
}

/// The four outfit colours; true when any is non-zero.
bool readOutfitColors(ClassicReader& r, OutfitColor* out, bool& any)
{
    uint8_t head = 0, body = 0, legs = 0, feet = 0;
    if (!r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet))
        return false;
    out->set_head(head);
    out->set_body(body);
    out->set_legs(legs);
    out->set_feet(feet);
    any = head != 0 || body != 0 || legs != 0 || feet != 0;
    return true;
}

/// The icons of a creature: u8 count × (u8 icon, u8 category, u16 value, bool
/// flag); category 0 and 1 name the icon's field, others leave it out.
bool readCreatureIcons(ClassicReader& r, CreatureData* out)
{
    out->set_update_icons(true);
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        CreatureOtherIcons* entry = out->add_icons();
        uint8_t icon = 0, category = 0;
        uint16_t value = 0;
        bool flag = false;
        if (!r.u8(icon) || !r.u8(category))
            return false;
        if (category == 1)
            entry->set_special_icon(icon);
        else if (category == 0)
            entry->set_icon(icon);
        if (!r.u16(value) || !readBool(r, flag))
            return false;
        entry->set_value(value);
        entry->set_flag(flag);
    }
    return true;
}

/// The second icon list: u8 count × u8.
bool readExtraIcons(ClassicReader& r, CreatureData* out)
{
    out->set_update_extra_icons(true);
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t icon = 0;
        if (!r.u8(icon))
            return false;
        out->add_extra_icons(icon);
    }
    return true;
}

/// The u32 id of a known creature: the message names it as both the creature
/// and the one it is stored under.
bool readKnownCreatureId(ClassicReader& r, CreatureData* out)
{
    uint32_t id = 0;
    if (!r.u32(id))
        return false;
    out->set_remove_id(id);
    out->set_id(id);
    return true;
}

/// The extra data of an object, by the first appearance kind it belongs to.
bool readObjectExtraData(ClassicReader& r, AppearanceInstance* out)
{
    const uint16_t id = static_cast<uint16_t>(out->id());
    const auto& special = tibia::appearances::TAppearancesManager::instance().specialAppearancesForProtocolParsing();
    uint8_t value = 0;
    if (special.isCumulative(id)) {
        if (!r.u8(value))
            return false;
        out->MutableExtension(CumulativeObject::cumulative_object)->set_count(value);
        return true;
    }
    if (special.isLiquid(id)) {
        if (!r.u8(value))
            return false;
        out->MutableExtension(LiquidObject::liquid_object)->set_liquid(value);
        return true;
    }
    if (special.isContainer(id)) {
        uint8_t flags = 0;
        if (!r.u8(flags))
            return false;
        if (flags == 0)
            return true;
        ContainerObject* container = out->MutableExtension(ContainerObject::container_object);
        container->set_flags(flags);
        uint32_t data = 0;
        if (flags & 1) {
            if (!r.u32(data))
                return false;
            container->set_loot_flags(data);
        }
        if (flags & 2) {
            if (!r.u32(data))
                return false;
            container->set_ammo_total(data);
        }
        if (flags & 8) {
            if (!r.u32(data))
                return false;
            container->set_obtain_flags(data);
        }
        return true;
    }
    if (special.isShowOffSocket(id)) {
        ShowOffSocketObject* socket = out->MutableExtension(ShowOffSocketObject::show_off_socket_object);
        if (!readOutfit(r, socket->mutable_outfit(), true, true, true))
            return false;
        // A socket without a look type keeps no look type.
        if (socket->outfit().look_type() == 0)
            socket->mutable_outfit()->clear_look_type();
        bool visible = false;
        if (!readOutfit(r, socket->mutable_mount(), true, false, false) || !r.u8(value) || !readBool(r, visible))
            return false;
        socket->set_direction(value);
        socket->set_visible(visible);
        return true;
    }
    if (special.isUpgradeable(id)) {
        if (!r.u8(value))
            return false;
        out->MutableExtension(UpgradeableObject::upgradeable_object)->set_tier(value);
        return true;
    }
    uint32_t number = 0;
    bool brandNew = false;
    if (special.isWearOut(id)) {
        if (!r.u32(number) || !readBool(r, brandNew))
            return false;
        WearoutObject* wearout = out->MutableExtension(WearoutObject::wearout_object);
        wearout->set_charges(number);
        wearout->set_brand_new(brandNew);
        return true;
    }
    const bool expireStop = special.isExpireStop(id);
    if (expireStop || special.isExpire(id)) {
        if (!r.u32(number) || !readBool(r, brandNew))
            return false;
        ExpireObject* expire = out->MutableExtension(ExpireObject::expire_object);
        expire->set_duration(number);
        expire->set_brand_new(brandNew);
        expire->set_expire_stop(expireStop);
        return true;
    }
    if (special.isDecoKit(id)) {
        uint16_t deco = 0;
        if (!r.u16(deco))
            return false;
        out->MutableExtension(DecoItemKitObject::deco_item_kit_object)->set_deco_item_id(deco);
    }
    return true;
}

/// The fields of `area`'s volume in the server's order: each described field
/// with its coordinate, the skipped ones left out (CIP's area reader).
bool readArea(ClassicReader& r, MapArea* area, const TWorldMapCentralPosition& central, const TShearedWorldMapCuboid& volume)
{
    const std::size_t count = volume.getExtent().volume();
    MapFields* fields = area->mutable_field_data()->mutable_fields();
    for (std::size_t index = 0; index < count; ++index) {
        MapField* field = fields->add_fields();
        setCoordinate(field->MutableExtension(MapFieldExtraData::map_field_extra_data)->mutable_coordinate(),
                      central.coordinateOfAreaIndex(volume, index));
        if (!readMapField(r, field))
            return false;
        index += field->skip();
        // The skip counts must stay inside the area.
        if (index > count)
            return false;
    }
    return true;
}

/// A mapped thing of a delete: a field and stack position, or 0xffff and a creature id.
bool readObjectPosition(ClassicReader& r, GameserverMessageDeleteOnMap* m)
{
    uint16_t x = 0;
    if (!r.u16(x))
        return false;
    if (x == 0xffff) {
        uint32_t id = 0;
        if (!r.u32(id))
            return false;
        m->set_creature_id(id);
        return true;
    }
    uint16_t y = 0;
    uint8_t z = 0, stack = 0;
    if (!r.u16(y) || !r.u8(z))
        return false;
    WorldmapObjectPosition* position = m->mutable_object_position();
    setCoordinate(position->mutable_coordinate(), x, y, z);
    if (!r.u8(stack))
        return false;
    position->set_stack_position(stack);
    return true;
}

}

bool readOutfit(ClassicReader& r, ProtocolOutfit* out, const bool outfit)
{
    uint16_t lookType = 0;
    if (!r.u16(lookType))
        return false;
    out->set_look_type(lookType);
    if (lookType != 0) {
        uint8_t head = 0, body = 0, legs = 0, feet = 0;
        if (!r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet))
            return false;
        // All four colours zero means the outfit has no colours.
        if (head != 0 || body != 0 || legs != 0 || feet != 0) {
            out->set_head(head);
            out->set_body(body);
            out->set_legs(legs);
            out->set_feet(feet);
        }
        if (outfit) {
            uint8_t addons = 0;
            if (!r.u8(addons))
                return false;
            out->set_addons(addons);
        }
        return true;
    }
    if (outfit) {
        uint16_t item = 0;
        if (!r.u16(item))
            return false;
        out->set_look_type_ex(item);
    }
    return true;
}

bool readOutfit(ClassicReader& r, Outfit* out, const bool colors, const bool addons, const bool object)
{
    uint16_t lookType = 0;
    if (!r.u16(lookType))
        return false;
    if (lookType == 0) {
        if (!object)
            return true;
        uint16_t objectType = 0;
        if (!r.u16(objectType))
            return false;
        if (objectType == 0)
            out->set_look_type(0);
        else
            out->set_object_type(objectType);
        return true;
    }
    out->set_look_type(lookType);
    if (colors) {
        bool any = false;
        if (!readOutfitColors(r, out->mutable_colors(), any))
            return false;
        // Four zero colours: the outfit has none.
        if (!any)
            out->clear_colors();
    }
    if (addons) {
        uint8_t value = 0;
        if (!r.u8(value))
            return false;
        if (value != 0)
            out->set_addons(value);
    }
    return true;
}

bool readCreature(ClassicReader& r, CreatureData* out, const uint16_t kind)
{
    uint32_t value = 0;
    if (kind == NewCreature) {
        // The creature a new one replaces (none: 0).
        if (!r.u32(value))
            return false;
        if (value != 0)
            out->set_remove_id(value);
    }
    uint32_t id = 0;
    if (!r.u32(id))
        return false;
    out->set_id(id);
    uint8_t byte = 0;
    if (kind == KnownCreature || kind == TurnedCreature) {
        out->set_remove_id(id);
    } else if (kind == NewCreature) {
        if (!r.u8(byte))
            return false;
        out->set_type(byte);
        if (byte == 3) {
            if (!r.u32(value))
                return false;
            out->set_master_id(value);
        }
        if (!r.string(*out->mutable_name()))
            return false;
    }
    if (kind != NewCreature && kind != KnownCreature) {
        // A creature that turned: its direction.
        if (!r.u8(byte))
            return false;
        out->set_direction(byte);
    } else {
        if (!r.u8(byte))
            return false;
        out->set_health_percent(byte);
        if (!r.u8(byte))
            return false;
        out->set_direction(byte);
        if (!readOutfit(r, out->mutable_outfit(), true, true, true) || !readOutfit(r, out->mutable_mount(), true, false, false))
            return false;
        uint8_t intensity = 0, color = 0;
        uint16_t speed = 0;
        if (!r.u8(intensity) || !r.u8(color) || !r.u16(speed))
            return false;
        out->set_light_intensity(intensity);
        out->set_light_color(color);
        out->set_speed(speed);
        if (!readCreatureIcons(r, out) || !readExtraIcons(r, out))
            return false;
        uint8_t skull = 0, shield = 0;
        if (!r.u8(skull) || !r.u8(shield))
            return false;
        out->set_skull(skull);
        out->set_shield(shield);
        if (kind == NewCreature) {
            if (!r.u8(byte))
                return false;
            out->set_emblem(byte);
        }
        if (!r.u8(byte))
            return false;
        out->set_type(byte);
        if (byte == 0) {
            uint8_t vocation = 0;
            if (!r.u8(vocation))
                return false;
            out->set_vocation(vocation % 10);
        } else if (byte == 3) {
            if (!r.u32(value))
                return false;
            out->set_master_id(value);
        }
        uint8_t icon = 0, mark = 0, inspection = 0;
        if (!r.u8(icon) || !r.u8(mark) || !r.u8(inspection))
            return false;
        out->set_icon(icon);
        out->set_mark(mark);
        out->set_inspection_type(inspection);
    }
    bool unpassable = false;
    if (!readBool(r, unpassable))
        return false;
    out->set_unpassable(unpassable);
    return true;
}

bool readMapObject(ClassicReader& r, AppearanceInstance* out, uint16_t id)
{
    if (id == 0 && !r.u16(id))
        return false;
    out->set_id(id);
    if (id >= NewCreature && id <= TurnedCreature)
        return readCreature(r, out->MutableExtension(CreatureData::creature_data), id);
    return readObjectExtraData(r, out);
}

bool readMapField(ClassicReader& r, MapField* out)
{
    // Objects until a value of 0xff00 or more; its low byte is the number of
    // fields to skip after this one.
    uint16_t id = 0;
    if (!r.u16(id))
        return false;
    while (id < 0xff00) {
        if (id == 0)
            return false; // a field has no zero-id appearance
        if (!readMapObject(r, out->add_objects(), id) || !r.u16(id))
            return false;
    }
    if ((id & 0xff) != 0)
        out->set_skip(id & 0xff);
    return true;
}

bool isMapMessage(const int opcode)
{
    switch (opcode) {
    case 100: case 101: case 102: case 103: case 104:
    case 105: case 106: case 107: case 108:
    case 190: case 191:
        return true;
    default:
        return false;
    }
}

size_t fillMapMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    // The slices and floor changes move the central position first; their
    // fields cover the area the move added.
    TWorldMapCentralPosition central = centralPositionAt(g_center);
    const auto fillArea = [&](MapArea* area, const TShearedWorldMapCuboid& volume) -> size_t {
        g_pendingCenter = central.center;
        return readArea(r, area, central, volume) ? r.i : 0;
    };
    switch (opcode) {
    case 100: {
        auto* m = msg.MutableExtension(GameserverMessageFullMap::gameserver_message_full_map);
        TWorldMapCoordinate center;
        if (!readPosition(r, center))
            return 0;
        central = centralPositionAt(center);
        setCoordinate(m->mutable_coordinate(), center);
        return fillArea(m->mutable_area(), central.visibleArea());
    }
    case 101:
        central.moveUp();
        return fillArea(msg.MutableExtension(GameserverMessageTopRow::gameserver_message_top_row)->mutable_area(),
                        central.topRow());
    case 102:
        central.moveRight();
        return fillArea(msg.MutableExtension(GameserverMessageRightColumn::gameserver_message_right_column)->mutable_area(),
                        central.rightColumn());
    case 103:
        central.moveDown();
        return fillArea(msg.MutableExtension(GameserverMessageBottomRow::gameserver_message_bottom_row)->mutable_area(),
                        central.bottomRow());
    case 104:
        central.moveLeft();
        return fillArea(msg.MutableExtension(GameserverMessageLeftColumn::gameserver_message_left_column)->mutable_area(),
                        central.leftColumn());
    case 105: {
        // One field: an area of one field at the coordinate.
        auto* m = msg.MutableExtension(GameserverMessageFieldData::gameserver_message_field_data);
        TWorldMapCoordinate at;
        if (!readPosition(r, at))
            return 0;
        setCoordinate(m->mutable_coordinate(), at);
        const TShearedWorldMapCuboid field(TWorldMapCuboid::ofField(at), at.z);
        return fillArea(m->mutable_area(), field);
    }
    case 106: {
        auto* m = msg.MutableExtension(GameserverMessageCreateOnMap::gameserver_message_create_on_map);
        TWorldMapCoordinate at;
        uint8_t stack = 0;
        if (!readPosition(r, at) || !r.u8(stack))
            return 0;
        setCoordinate(m->mutable_coordinate(), at);
        m->set_stack_position(stack);
        g_pendingCenter = g_center;
        return readMapObject(r, m->mutable_object()) ? r.i : 0;
    }
    case 107: {
        auto* m = msg.MutableExtension(GameserverMessageChangeOnMap::gameserver_message_change_on_map);
        g_pendingCenter = g_center;
        uint16_t x = 0;
        if (!r.u16(x))
            return 0;
        if (x == 0xffff) {
            // The creature-id form: the object's creature data names the creature.
            uint32_t id = 0;
            if (!r.u32(id) || !readMapObject(r, m->mutable_object()))
                return 0;
            m->mutable_object()->MutableExtension(CreatureData::creature_data)->set_id(id);
            return r.i;
        }
        uint16_t y = 0;
        uint8_t z = 0, stack = 0;
        if (!r.u16(y) || !r.u8(z) || !r.u8(stack))
            return 0;
        setCoordinate(m->mutable_coordinate(), x, y, z);
        m->set_stack_position(stack);
        return readMapObject(r, m->mutable_object()) ? r.i : 0;
    }
    case 108:
        g_pendingCenter = g_center;
        return readObjectPosition(r, msg.MutableExtension(GameserverMessageDeleteOnMap::gameserver_message_delete_on_map))
                   ? r.i
                   : 0;
    case 190:
        central.moveUpFloor();
        return fillArea(msg.MutableExtension(GameserverMessageTopFloor::gameserver_message_top_floor)->mutable_area(),
                        central.topFloorArea());
    case 191:
        central.moveDownFloor();
        return fillArea(msg.MutableExtension(GameserverMessageBottomFloor::gameserver_message_bottom_floor)->mutable_area(),
                        central.bottomFloorArea());
    default:
        return 0;
    }
}

void setMapViewport(const int x, const int y, const int z)
{
    g_center = TWorldMapCoordinate(x, y, z);
    g_pendingCenter = g_center;
}

void advanceMapViewport(const GameserverMessage&)
{
    g_center = g_pendingCenter;
}

bool isCreatureMessage(const int opcode)
{
    switch (opcode) {
    case 119: // InspectionState
    case 139: // CreatureUpdate
    case 140: // CreatureHealth
    case 141: // CreatureLight
    case 142: // CreatureOutfit
    case 143: // CreatureSpeed
    case 144: // CreatureSkull
    case 145: // CreatureParty
    case 146: // CreatureUnpass
    case 147: // CreatureMarks
    case 149: // CreatureType
    case 181: // SnapBack
    case 182: // Wait
        return true;
    default:
        return false;
    }
}

size_t fillCreatureMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 139: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureUpdate::gameserver_message_creature_update);
        uint32_t id = 0;
        uint8_t type = 0;
        if (!r.u32(id) || !r.u8(type))
            return 0;
        // The update names a known creature: its id and the id it is stored under.
        CreatureData* creature = m->mutable_creature();
        creature->set_id(id);
        creature->set_remove_id(id);
        m->set_type(type);
        switch (type) {
        case 0: {
            creature->clear_remove_id();
            uint16_t kind = 0;
            if (!r.u16(kind))
                return 0;
            if (kind >= NewCreature && kind <= TurnedCreature && !readCreature(r, creature, kind))
                return 0;
            break;
        }
        // One field of the known creature.
        case 11: {
            uint8_t mana = 0;
            if (!r.u8(mana))
                return 0;
            creature->set_mana_percent(mana);
            break;
        }
        case 12: {
            bool show = false;
            if (!readBool(r, show))
                return 0;
            creature->set_show_status(show);
            break;
        }
        case 13: {
            uint8_t vocation = 0;
            if (!r.u8(vocation))
                return 0;
            creature->set_vocation(vocation % 10);
            break;
        }
        case 14:
            if (!readCreatureIcons(r, creature))
                return 0;
            break;
        case 15:
            if (!readExtraIcons(r, creature))
                return 0;
            break;
        // Product server party extensions.
        case 16:
        case 17: {
            uint8_t percent = 0;
            uint32_t value = 0, maximum = 0;
            if (!r.u8(percent) || !r.u32(value) || !r.u32(maximum))
                return 0;
            if (type == 16) {
                m->set_health_percent(percent);
                m->set_health(value);
                m->set_max_health(maximum);
            } else {
                m->set_mana_percent(percent);
                m->set_mana(value);
                m->set_max_mana(maximum);
            }
            break;
        }
        case 18: {
            uint8_t vocation = 0;
            uint16_t level = 0;
            if (!r.u8(vocation) || !r.u16(level))
                return 0;
            m->set_vocation(vocation);
            m->set_level(level);
            break;
        }
        default:
            break;
        }
        return r.i;
    }
    case 119: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageInspectionState::gameserver_message_inspection_state)->mutable_creature();
        uint8_t state = 0;
        if (!readKnownCreatureId(r, creature) || !r.u8(state))
            return 0;
        creature->set_inspection_type(state);
        return r.i;
    }
    case 140: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health)->mutable_creature();
        uint8_t health = 0;
        if (!readKnownCreatureId(r, creature) || !r.u8(health))
            return 0;
        creature->set_health_percent(health);
        return r.i;
    }
    case 141: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureLight::gameserver_message_creature_light)->mutable_creature();
        uint8_t intensity = 0, color = 0;
        if (!readKnownCreatureId(r, creature) || !r.u8(intensity))
            return 0;
        creature->set_light_intensity(intensity);
        if (!r.u8(color))
            return 0;
        creature->set_light_color(color);
        return r.i;
    }
    case 142: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit)->mutable_creature();
        // The outfit with colours, addons or its object; the mount with colours.
        if (!readKnownCreatureId(r, creature) || !readOutfit(r, creature->mutable_outfit(), true, true, true)
            || !readOutfit(r, creature->mutable_mount(), true, false, false))
            return 0;
        return r.i;
    }
    case 143: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
        // The first speed is the message's own, the second the creature's.
        uint16_t baseSpeed = 0, speed = 0;
        if (!readKnownCreatureId(r, m->mutable_creature()) || !r.u16(baseSpeed) || !r.u16(speed))
            return 0;
        m->mutable_creature()->set_speed(speed);
        m->set_base_speed(baseSpeed);
        return r.i;
    }
    case 144: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull)->mutable_creature();
        uint8_t skull = 0;
        if (!readKnownCreatureId(r, creature) || !r.u8(skull))
            return 0;
        creature->set_skull(skull);
        return r.i;
    }
    case 145: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureParty::gameserver_message_creature_party)->mutable_creature();
        uint8_t shield = 0;
        if (!readKnownCreatureId(r, creature) || !r.u8(shield))
            return 0;
        creature->set_shield(shield);
        return r.i;
    }
    case 146: {
        CreatureData* creature =
            msg.MutableExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass)->mutable_creature();
        bool unpassable = false;
        if (!readKnownCreatureId(r, creature) || !readBool(r, unpassable))
            return 0;
        creature->set_unpassable(unpassable);
        return r.i;
    }
    case 147: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureMarks::gameserver_message_creature_marks);
        uint32_t id = 0;
        uint8_t kind = 0;
        if (!r.u32(id) || !r.u8(kind))
            return 0;
        m->set_creature_id(id);
        m->set_kind(kind);
        if (kind >= 1 && kind <= 3) {
            uint8_t value = 0;
            if (!r.u8(value))
                return 0;
            m->set_value(value);
        }
        return r.i;
    }
    case 149: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureType::gameserver_message_creature_type);
        uint32_t id = 0;
        uint8_t type = 0;
        if (!r.u32(id) || !r.u8(type))
            return 0;
        m->set_creature_id(id);
        m->set_type(type);
        if (type == 3) {
            uint32_t master = 0;
            if (!r.u32(master))
                return 0;
            m->set_master_id(master);
        }
        return r.i;
    }
    case 181: {
        auto* m = msg.MutableExtension(GameserverMessageSnapBack::gameserver_message_snap_back);
        uint8_t direction = 0;
        if (!r.u8(direction))
            return 0;
        m->set_direction(direction);
        return r.i;
    }
    case 182: {
        auto* m = msg.MutableExtension(GameserverMessageWait::gameserver_message_wait);
        uint16_t time = 0;
        if (!r.u16(time))
            return 0;
        m->set_time(time);
        return r.i;
    }
    default:
        return 0;
    }
}

}
