#include "protocol/inventoryitemwire.h"

#include <client/const.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::protocol {

using ::tibia::protobuf::protocol::ProtocolInventoryItem;

namespace {

// Container flag bits: each set bit is followed by one u32, in bit order.
constexpr uint8_t ContainerLootFlags = 0x01;
constexpr uint8_t ContainerAmmoTotal = 0x02;
constexpr uint8_t ContainerObtainFlags = 0x08;

bool readOutfitLook(ClassicReader& r, ProtocolInventoryItem* out)
{
    uint16_t lookType = 0;
    if (!r.u16(lookType))
        return false;
    out->set_look_type(lookType);
    if (lookType != 0) {
        uint8_t head = 0, body = 0, legs = 0, feet = 0, addons = 0;
        if (!r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet) || !r.u8(addons))
            return false;
        out->set_look_head(head);
        out->set_look_body(body);
        out->set_look_legs(legs);
        out->set_look_feet(feet);
        out->set_look_addons(addons);
        return true;
    }
    uint16_t lookTypeEx = 0;
    if (!r.u16(lookTypeEx))
        return false;
    out->set_look_type_ex(lookTypeEx);
    return true;
}

bool readMountLook(ClassicReader& r, ProtocolInventoryItem* out)
{
    uint16_t lookMount = 0;
    if (!r.u16(lookMount))
        return false;
    out->set_look_mount(lookMount);
    if (lookMount == 0)
        return true;
    uint8_t head = 0, body = 0, legs = 0, feet = 0;
    if (!r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet))
        return false;
    out->set_look_mount_head(head);
    out->set_look_mount_body(body);
    out->set_look_mount_legs(legs);
    out->set_look_mount_feet(feet);
    return true;
}

}

bool readInventoryItem(ClassicReader& r, ProtocolInventoryItem* out, int presetItemId)
{
    if (!out)
        return false;

    out->Clear();

    uint16_t id = static_cast<uint16_t>(presetItemId);
    if (presetItemId == 0) {
        if (!r.u16(id))
            return false;
    }
    out->set_item_id(id);

    // The appearance kinds that carry extra data are exclusive: the first kind
    // the appearance belongs to decides what follows. An appearance of no such
    // kind (or one the client does not know) has no extra data.
    const ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return true;

    if (type->isStackable() || type->isFluidContainer() || type->isSplash()) {
        uint8_t count = 0;
        if (!r.u8(count))
            return false;
        out->set_count_or_subtype(count);
        return true;
    }

    if (type->isContainer()) {
        uint8_t flags = 0;
        if (!r.u8(flags))
            return false;
        if (flags == 0)
            return true;
        out->set_container_type(flags);
        uint32_t value = 0;
        if (flags & ContainerLootFlags) {
            if (!r.u32(value))
                return false;
            out->set_loot_flags(value);
        }
        if (flags & ContainerAmmoTotal) {
            if (!r.u32(value))
                return false;
            out->set_container_ammo_total(value);
            out->set_has_container_ammo(true);
        }
        if (flags & ContainerObtainFlags) {
            if (!r.u32(value))
                return false;
            out->set_obtain_flags(value);
        }
        return true;
    }

    if (type->isPodium()) {
        out->set_has_podium(true);
        uint8_t direction = 0, visible = 0;
        if (!readOutfitLook(r, out) || !readMountLook(r, out) || !r.u8(direction) || !r.u8(visible))
            return false;
        out->set_podium_direction(direction);
        out->set_podium_visible(visible != 0);
        return true;
    }

    if (type->getClassification() > 0) {
        uint8_t tier = 0;
        if (!r.u8(tier))
            return false;
        out->set_tier(tier);
        return true;
    }

    if (type->hasWearOut()) {
        uint32_t charges = 0;
        uint8_t brandNew = 0;
        if (!r.u32(charges) || !r.u8(brandNew))
            return false;
        out->set_charges(charges);
        out->set_brand_new(brandNew != 0);
        return true;
    }

    if (type->hasExpireStop() || type->hasExpire() || type->hasClockExpire()) {
        uint32_t duration = 0;
        uint8_t brandNew = 0;
        if (!r.u32(duration) || !r.u8(brandNew))
            return false;
        out->set_duration_seconds(duration);
        out->set_brand_new(brandNew != 0);
        return true;
    }

    if (type->isDecoKit()) {
        uint16_t deco = 0;
        if (!r.u16(deco))
            return false;
        out->set_deco_item_id(deco);
    }
    return true;
}

void writeInventoryItem(ClassicWriter& w, const ProtocolInventoryItem& item)
{
    const uint16_t id = static_cast<uint16_t>(item.item_id());
    w.u16(id);

    const ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return;

    if (type->isStackable() || type->isFluidContainer() || type->isSplash()) {
        w.u8(uint8_t(item.count_or_subtype()));
    } else if (type->isContainer()) {
        const uint8_t flags = uint8_t(item.container_type());
        w.u8(flags);
        if (flags & ContainerLootFlags)
            w.u32(item.loot_flags());
        if (flags & ContainerAmmoTotal)
            w.u32(item.container_ammo_total());
        if (flags & ContainerObtainFlags)
            w.u32(item.obtain_flags());
    } else if (type->isPodium()) {
        const uint16_t looktype = static_cast<uint16_t>(item.look_type());
        w.u16(looktype);
        if (looktype != 0) {
            w.u8(uint8_t(item.look_head()));
            w.u8(uint8_t(item.look_body()));
            w.u8(uint8_t(item.look_legs()));
            w.u8(uint8_t(item.look_feet()));
            w.u8(uint8_t(item.look_addons()));
        } else {
            w.u16(uint16_t(item.look_type_ex()));
        }
        const uint16_t lookmount = static_cast<uint16_t>(item.look_mount());
        w.u16(lookmount);
        if (lookmount != 0) {
            w.u8(uint8_t(item.look_mount_head()));
            w.u8(uint8_t(item.look_mount_body()));
            w.u8(uint8_t(item.look_mount_legs()));
            w.u8(uint8_t(item.look_mount_feet()));
        }
        w.u8(uint8_t(item.podium_direction()));
        w.u8(item.podium_visible() ? 1 : 0);
    } else if (type->getClassification() > 0) {
        w.u8(uint8_t(item.tier()));
    } else if (type->hasWearOut()) {
        w.u32(item.charges());
        w.u8(item.brand_new() ? 1 : 0);
    } else if (type->hasExpireStop() || type->hasExpire() || type->hasClockExpire()) {
        w.u32(item.duration_seconds());
        w.u8(item.brand_new() ? 1 : 0);
    } else if (type->isDecoKit()) {
        w.u16(uint16_t(item.deco_item_id()));
    }
}

} // namespace tibia::protocol
