#include "protocol/mapwire.h"

#include "protocol/inventoryitemwire.h"

#include <client/const.h>
#include <client/position.h>
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/mapsession.h>

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

constexpr uint16_t NewCreature = 0x61;
constexpr uint16_t KnownCreature = 0x62;
constexpr uint16_t TurnedCreature = 0x63;

/// The translator's view: the centre of the fields the next map message describes.
Position g_center;

void setCoordinate(ProtocolCoordinate* c, const int x, const int y, const int z)
{
    c->set_x(static_cast<uint32_t>(x));
    c->set_y(static_cast<uint32_t>(y));
    c->set_z(static_cast<uint32_t>(z));
}

bool readPosition(ClassicReader& r, Position& out)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (!r.u16(x) || !r.u16(y) || !r.u8(z))
        return false;
    out = Position(x, y, z);
    return true;
}

bool feature(const tibia::client::GameFeature f)
{
    return tibia::game::cipFeature(f);
}

/// The icons of a creature: u8 count × (u8 icon, u8 category, u16 value, bool flag).
bool readCreatureIcons(ClassicReader& r, google::protobuf::RepeatedPtrField<ProtocolCreatureIcon>* icons)
{
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t icon = 0, category = 0, flag = 0;
        uint16_t value = 0;
        if (!r.u8(icon) || !r.u8(category) || !r.u16(value) || !r.u8(flag))
            return false;
        ProtocolCreatureIcon* entry = icons->Add();
        entry->set_icon(icon);
        entry->set_category(category);
        entry->set_value(value);
        entry->set_flag(flag != 0);
    }
    return true;
}

/// The second icon list: u8 count × u8.
bool readExtraIcons(ClassicReader& r, google::protobuf::RepeatedField<uint32_t>* icons)
{
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t icon = 0;
        if (!r.u8(icon))
            return false;
        icons->Add(icon);
    }
    return true;
}

/// The fields of one floor, column by column, carrying the skip count over.
bool readFloor(ClassicReader& r, google::protobuf::RepeatedPtrField<ProtocolMapField>* fields, const int x, const int y,
               const int z, const int width, const int height, const int offset, int& skip);

}

bool readMapFloor(ClassicReader& r, google::protobuf::RepeatedPtrField<ProtocolMapField>* fields, const int x, const int y,
                  const int z, const int width, const int height, const int offset, int& skip)
{
    return readFloor(r, fields, x, y, z, width, height, offset, skip);
}

namespace {

bool readFloor(ClassicReader& r, google::protobuf::RepeatedPtrField<ProtocolMapField>* fields, const int x, const int y,
               const int z, const int width, const int height, const int offset, int& skip)
{
    for (int nx = 0; nx < width; ++nx) {
        for (int ny = 0; ny < height; ++ny) {
            ProtocolMapField* field = fields->Add();
            setCoordinate(field->mutable_coordinate(), x + nx + offset, y + ny + offset, z);
            if (skip > 0) {
                --skip;
                continue;
            }
            if (!readMapField(r, field, skip))
                return false;
        }
    }
    return true;
}

/// An area through every floor the view shows from `z`.
bool readArea(ClassicReader& r, google::protobuf::RepeatedPtrField<ProtocolMapField>* fields, const int x, const int y,
              const int z, const int width, const int height)
{
    int startZ = 0;
    int endZ = 0;
    int step = 0;
    if (z > tibia::config::mapSeaFloor()) {
        startZ = z - tibia::config::mapAwareUndergroundFloorRange();
        endZ = std::min<int>(z + tibia::config::mapAwareUndergroundFloorRange(), tibia::config::mapMaxZ());
        step = 1;
    } else {
        startZ = tibia::config::mapSeaFloor();
        endZ = 0;
        step = -1;
    }
    int skip = 0;
    for (int nz = startZ; nz != endZ + step; nz += step) {
        if (!readFloor(r, fields, x, y, nz, width, height, z - nz, skip))
            return false;
    }
    return true;
}

/// A mapped thing: a field and stack position, or 0xffff and a creature id.
template<typename Message>
bool readMappedThing(ClassicReader& r, Message* m)
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
    if (!r.u16(y) || !r.u8(z) || !r.u8(stack))
        return false;
    setCoordinate(m->mutable_coordinate(), x, y, z);
    m->set_stackpos(stack);
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
        out->set_head(head);
        out->set_body(body);
        out->set_legs(legs);
        out->set_feet(feet);
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

bool readCreature(ClassicReader& r, ProtocolCreature* out, const uint16_t kind)
{
    out->set_kind(kind);
    uint32_t id = 0;
    if (kind == NewCreature) {
        uint32_t removeId = 0;
        if (!r.u32(removeId))
            return false;
        out->set_remove_id(removeId);
    }
    if (!r.u32(id))
        return false;
    out->set_id(id);
    if (kind == TurnedCreature) {
        uint8_t direction = 0, unpassable = 0;
        if (!r.u8(direction) || !r.u8(unpassable))
            return false;
        out->set_direction(direction);
        out->set_unpassable(unpassable != 0);
        return true;
    }
    if (kind == NewCreature) {
        uint8_t type = 0;
        if (!r.u8(type))
            return false;
        out->set_type(type);
        if (type == 3) {
            uint32_t master = 0;
            if (!r.u32(master))
                return false;
            out->set_master_id(master);
        }
        std::string name;
        if (!r.string(name))
            return false;
        out->set_name(name);
    }
    uint8_t health = 0, direction = 0;
    if (!r.u8(health) || !r.u8(direction))
        return false;
    out->set_health_percent(health);
    out->set_direction(direction);
    if (!readOutfit(r, out->mutable_outfit(), true) || !readOutfit(r, out->mutable_mount(), false))
        return false;
    uint8_t intensity = 0, color = 0;
    uint16_t speed = 0;
    if (!r.u8(intensity) || !r.u8(color) || !r.u16(speed))
        return false;
    out->set_light_intensity(intensity);
    out->set_light_color(color);
    out->set_speed(speed);
    if (!readCreatureIcons(r, out->mutable_icons()) || !readExtraIcons(r, out->mutable_extra_icons()))
        return false;
    uint8_t skull = 0, shield = 0;
    if (!r.u8(skull) || !r.u8(shield))
        return false;
    out->set_skull(skull);
    out->set_shield(shield);
    if (kind == NewCreature) {
        uint8_t emblem = 0;
        if (!r.u8(emblem))
            return false;
        out->set_emblem(emblem);
    }
    uint8_t type = 0;
    if (!r.u8(type))
        return false;
    out->set_type(type);
    if (type == 0) {
        uint8_t vocation = 0;
        if (!r.u8(vocation))
            return false;
        out->set_vocation(vocation % 10);
    } else if (type == 3) {
        uint32_t master = 0;
        if (!r.u32(master))
            return false;
        out->set_master_id(master);
    }
    uint8_t icon = 0, mark = 0, inspection = 0, unpassable = 0;
    if (!r.u8(icon) || !r.u8(mark) || !r.u8(inspection) || !r.u8(unpassable))
        return false;
    out->set_icon(icon);
    out->set_mark(mark);
    out->set_inspection_type(inspection);
    out->set_unpassable(unpassable != 0);
    return true;
}

bool readMapObject(ClassicReader& r, ProtocolMapObject* out)
{
    uint16_t id = 0;
    if (!r.u16(id) || id == 0)
        return false;
    if (id == NewCreature || id == KnownCreature || id == TurnedCreature)
        return readCreature(r, out->mutable_creature(), id);
    return readInventoryItem(r, out->mutable_item(), id);
}

bool readMapField(ClassicReader& r, ProtocolMapField* out, int& skip)
{
    bool environmentEffect = feature(tibia::client::GameEnvironmentEffect);
    for (int stackPos = 0; stackPos < 256; ++stackPos) {
        if (r.remaining() < 2)
            return false;
        const uint16_t peek = uint16_t(r.p[r.i]) | (uint16_t(r.p[r.i + 1]) << 8);
        if (peek >= 0xff00) {
            r.i += 2;
            skip = peek & 0xff;
            return true;
        }
        if (environmentEffect) {
            uint16_t effect = 0;
            (void)r.u16(effect);
            out->set_environment_effect(effect);
            environmentEffect = false;
            continue;
        }
        if (!readMapObject(r, out->add_objects()))
            return false;
    }
    skip = 0;
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
    const tibia::game::MapAwareRange range = tibia::game::awareRange();
    const auto fillArea = [&](auto* m, const Position& center, const int x, const int y, const int width, const int height) -> size_t {
        setCoordinate(m->mutable_coordinate(), center.x, center.y, center.z);
        return readArea(r, m->mutable_fields(), x, y, center.z, width, height) ? r.i : 0;
    };
    switch (opcode) {
    case 100: {
        Position center;
        if (!readPosition(r, center))
            return 0;
        return fillArea(msg.MutableExtension(GameserverMessageFullMap::gameserver_message_full_map), center,
                        center.x - range.left, center.y - range.top, range.horizontal(), range.vertical());
    }
    case 101: {
        const Position center(g_center.x, g_center.y - 1, g_center.z);
        return fillArea(msg.MutableExtension(GameserverMessageTopRow::gameserver_message_top_row), center,
                        center.x - range.left, center.y - range.top, range.horizontal(), 1);
    }
    case 102: {
        const Position center(g_center.x + 1, g_center.y, g_center.z);
        return fillArea(msg.MutableExtension(GameserverMessageRightColumn::gameserver_message_right_column), center,
                        center.x + range.right, center.y - range.top, 1, range.vertical());
    }
    case 103: {
        const Position center(g_center.x, g_center.y + 1, g_center.z);
        return fillArea(msg.MutableExtension(GameserverMessageBottomRow::gameserver_message_bottom_row), center,
                        center.x - range.left, center.y + range.bottom, range.horizontal(), 1);
    }
    case 104: {
        const Position center(g_center.x - 1, g_center.y, g_center.z);
        return fillArea(msg.MutableExtension(GameserverMessageLeftColumn::gameserver_message_left_column), center,
                        center.x - range.left, center.y - range.top, 1, range.vertical());
    }
    case 105: {
        auto* m = msg.MutableExtension(GameserverMessageFieldData::gameserver_message_field_data);
        Position at;
        if (!readPosition(r, at))
            return 0;
        ProtocolMapField* field = m->mutable_field();
        setCoordinate(field->mutable_coordinate(), at.x, at.y, at.z);
        int skip = 0;
        return readMapField(r, field, skip) ? r.i : 0;
    }
    case 106: {
        auto* m = msg.MutableExtension(GameserverMessageCreateOnMap::gameserver_message_create_on_map);
        Position at;
        uint8_t stack = 0;
        if (!readPosition(r, at) || !r.u8(stack))
            return 0;
        setCoordinate(m->mutable_coordinate(), at.x, at.y, at.z);
        m->set_stackpos(stack);
        return readMapObject(r, m->mutable_object()) ? r.i : 0;
    }
    case 107: {
        auto* m = msg.MutableExtension(GameserverMessageChangeOnMap::gameserver_message_change_on_map);
        return readMappedThing(r, m) && readMapObject(r, m->mutable_object()) ? r.i : 0;
    }
    case 108: {
        auto* m = msg.MutableExtension(GameserverMessageDeleteOnMap::gameserver_message_delete_on_map);
        return readMappedThing(r, m) ? r.i : 0;
    }
    case 190: {
        // One floor up: the newly visible floors above, then the view moves +1,+1.
        auto* m = msg.MutableExtension(GameserverMessageTopFloor::gameserver_message_top_floor);
        const Position pos(g_center.x, g_center.y, g_center.z - 1);
        setCoordinate(m->mutable_coordinate(), pos.x + 1, pos.y + 1, pos.z);
        int skip = 0;
        if (pos.z == tibia::config::mapSeaFloor()) {
            for (int z = tibia::config::mapSeaFloor() - tibia::config::mapAwareUndergroundFloorRange(); z >= 0; --z) {
                if (!readFloor(r, m->mutable_fields(), pos.x - range.left, pos.y - range.top, z, range.horizontal(),
                               range.vertical(), 8 - z, skip))
                    return 0;
            }
        } else if (pos.z > tibia::config::mapSeaFloor()) {
            if (!readFloor(r, m->mutable_fields(), pos.x - range.left, pos.y - range.top,
                           pos.z - tibia::config::mapAwareUndergroundFloorRange(), range.horizontal(), range.vertical(), 3, skip))
                return 0;
        }
        return r.i;
    }
    case 191: {
        // One floor down: the newly visible floors below, then the view moves -1,-1.
        auto* m = msg.MutableExtension(GameserverMessageBottomFloor::gameserver_message_bottom_floor);
        const Position pos(g_center.x, g_center.y, g_center.z + 1);
        setCoordinate(m->mutable_coordinate(), pos.x - 1, pos.y - 1, pos.z);
        int skip = 0;
        if (pos.z == tibia::config::mapUndergroundFloorRange()) {
            for (int z = pos.z, offset = -1; z <= pos.z + tibia::config::mapAwareUndergroundFloorRange(); ++z, --offset) {
                if (!readFloor(r, m->mutable_fields(), pos.x - range.left, pos.y - range.top, z, range.horizontal(),
                               range.vertical(), offset, skip))
                    return 0;
            }
        } else if (pos.z > tibia::config::mapUndergroundFloorRange() && pos.z < tibia::config::mapMaxZ() - 1) {
            if (!readFloor(r, m->mutable_fields(), pos.x - range.left, pos.y - range.top,
                           pos.z + tibia::config::mapAwareUndergroundFloorRange(), range.horizontal(), range.vertical(), -3, skip))
                return 0;
        }
        return r.i;
    }
    default:
        return 0;
    }
}

void setMapViewport(const int x, const int y, const int z)
{
    g_center = Position(x, y, static_cast<uint8_t>(z));
}

void advanceMapViewport(const GameserverMessage& msg)
{
    const auto move = [](const auto& m) {
        g_center = Position(static_cast<int32_t>(m.coordinate().x()), static_cast<int32_t>(m.coordinate().y()),
                            static_cast<uint8_t>(m.coordinate().z()));
    };
    if (msg.HasExtension(GameserverMessageFullMap::gameserver_message_full_map))
        move(msg.GetExtension(GameserverMessageFullMap::gameserver_message_full_map));
    else if (msg.HasExtension(GameserverMessageTopRow::gameserver_message_top_row))
        move(msg.GetExtension(GameserverMessageTopRow::gameserver_message_top_row));
    else if (msg.HasExtension(GameserverMessageRightColumn::gameserver_message_right_column))
        move(msg.GetExtension(GameserverMessageRightColumn::gameserver_message_right_column));
    else if (msg.HasExtension(GameserverMessageBottomRow::gameserver_message_bottom_row))
        move(msg.GetExtension(GameserverMessageBottomRow::gameserver_message_bottom_row));
    else if (msg.HasExtension(GameserverMessageLeftColumn::gameserver_message_left_column))
        move(msg.GetExtension(GameserverMessageLeftColumn::gameserver_message_left_column));
    else if (msg.HasExtension(GameserverMessageTopFloor::gameserver_message_top_floor))
        move(msg.GetExtension(GameserverMessageTopFloor::gameserver_message_top_floor));
    else if (msg.HasExtension(GameserverMessageBottomFloor::gameserver_message_bottom_floor))
        move(msg.GetExtension(GameserverMessageBottomFloor::gameserver_message_bottom_floor));
}

bool isCreatureMessage(const int opcode)
{
    switch (opcode) {
    case 139: // CreatureUpdate
    case 142: // CreatureOutfit
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
        m->set_creature_id(id);
        m->set_type(type);
        switch (type) {
        case 0: {
            uint16_t kind = 0;
            if (!r.u16(kind))
                return 0;
            if (kind >= NewCreature && kind <= TurnedCreature && !readCreature(r, m->mutable_creature(), kind))
                return 0;
            break;
        }
        case 11: {
            uint8_t mana = 0;
            if (!r.u8(mana))
                return 0;
            m->set_mana_percent(mana);
            break;
        }
        case 12: {
            uint8_t show = 0;
            if (!r.u8(show))
                return 0;
            m->set_show_status(show != 0);
            break;
        }
        case 13: {
            uint8_t vocation = 0;
            if (!r.u8(vocation))
                return 0;
            m->set_vocation(vocation % 10);
            break;
        }
        case 14:
            if (!readCreatureIcons(r, m->mutable_icons()))
                return 0;
            break;
        case 15:
            if (!readExtraIcons(r, m->mutable_extra_icons()))
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
    case 142: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit);
        uint32_t id = 0;
        if (!r.u32(id))
            return 0;
        m->set_creature_id(id);
        if (!readOutfit(r, m->mutable_outfit(), true) || !readOutfit(r, m->mutable_mount(), false))
            return 0;
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
