#include "protocol/inventoryitemwire.h"

#include <client/const.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::protocol {

using ::tibia::protobuf::protocol::ProtocolInventoryItem;

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

    ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return false;

    if (g_game.getClientVersion() < 1281 && g_game.getFeature(tibia::client::GameThingMarks)) {
        uint8_t mark = 0;
        if (r.u8(mark))
            out->set_mark(mark);
    }

    if (type->isStackable() || type->isFluidContainer() || type->isSplash() || type->isChargeable()) {
        if (g_game.getFeature(tibia::client::GameCountU16)) {
            uint16_t count = 0;
            if (r.u16(count))
                out->set_count_or_subtype(count);
        } else {
            uint8_t count = 0;
            if (r.u8(count))
                out->set_count_or_subtype(count);
        }
    }

    if (g_game.getFeature(tibia::client::GameItemAnimationPhase) && type->getAnimationPhases() > 1) {
        uint8_t phase = 0;
        if (r.u8(phase))
            out->set_animation_phase(phase);
    }

    if (type->isContainer()) {
        if (g_game.getFeature(tibia::client::GameContainerTypes)) {
            uint8_t containerType = 0;
            if (!r.u8(containerType))
                return true;
            out->set_container_type(containerType);
            switch (containerType) {
            case 1: { // Loot Container
                uint32_t lootFlags = 0;
                if (r.u32(lootFlags))
                    out->set_loot_flags(lootFlags);
                break;
            }
            case 2: { // Content Counter
                uint32_t ammo = 0;
                if (r.u32(ammo)) {
                    out->set_container_ammo_total(ammo);
                    out->set_has_container_ammo(true);
                }
                break;
            }
            case 3: { // Manager Unknown
                uint32_t lootFlags = 0, obtainFlags = 0;
                if (r.u32(lootFlags))
                    out->set_loot_flags(lootFlags);
                if (r.u32(obtainFlags))
                    out->set_obtain_flags(obtainFlags);
                break;
            }
            case 4: // Loot Highlight
                break;
            case 8: { // Obtain
                uint32_t obtainFlags = 0;
                if (r.u32(obtainFlags))
                    out->set_obtain_flags(obtainFlags);
                break;
            }
            case 9: { // Manager
                uint32_t lootFlags = 0;
                if (r.u32(lootFlags))
                    out->set_loot_flags(lootFlags);
                if (g_game.getClientVersion() >= 1332) {
                    uint32_t obtainFlags = 0;
                    if (r.u32(obtainFlags))
                        out->set_obtain_flags(obtainFlags);
                }
                break;
            }
            case 11: { // Quiver Loot
                uint32_t lootFlags = 0, ammo = 0;
                if (r.u32(lootFlags))
                    out->set_loot_flags(lootFlags);
                if (r.u32(ammo)) {
                    out->set_container_ammo_total(ammo);
                    out->set_has_container_ammo(true);
                }
                if (g_game.getClientVersion() >= 1332) {
                    uint32_t obtainFlags = 0;
                    if (r.u32(obtainFlags))
                        out->set_obtain_flags(obtainFlags);
                }
                break;
            }
            default:
                break;
            }
        } else {
            if (g_game.getFeature(tibia::client::GameThingQuickLoot)) {
                uint8_t hasLoot = 0;
                if (r.u8(hasLoot) && hasLoot) {
                    uint32_t lootFlags = 0;
                    if (r.u32(lootFlags))
                        out->set_loot_flags(lootFlags);
                }
            }
            if (g_game.getFeature(tibia::client::GameThingQuiver)) {
                uint8_t hasAmmo = 0;
                if (r.u8(hasAmmo) && hasAmmo) {
                    uint32_t ammo = 0;
                    if (r.u32(ammo)) {
                        out->set_container_ammo_total(ammo);
                        out->set_has_container_ammo(true);
                    }
                }
            }
        }
    }

    if (g_game.getFeature(tibia::client::GameThingPodium) && type->isPodium()) {
        out->set_has_podium(true);
        uint16_t looktype = 0;
        if (!r.u16(looktype))
            return true;
        out->set_look_type(looktype);
        if (looktype != 0) {
            uint8_t head = 0, body = 0, legs = 0, feet = 0, addons = 0;
            if (r.u8(head))
                out->set_look_head(head);
            if (r.u8(body))
                out->set_look_body(body);
            if (r.u8(legs))
                out->set_look_legs(legs);
            if (r.u8(feet))
                out->set_look_feet(feet);
            if (r.u8(addons))
                out->set_look_addons(addons);
        } else if (g_game.getFeature(tibia::client::GameThingPodiumItemType)) {
            uint16_t lookTypeEx = 0;
            if (r.u16(lookTypeEx))
                out->set_look_type_ex(lookTypeEx);
        }

        uint16_t lookmount = 0;
        if (r.u16(lookmount))
            out->set_look_mount(lookmount);
        if (lookmount != 0) {
            uint8_t head = 0, body = 0, legs = 0, feet = 0;
            if (r.u8(head))
                out->set_look_mount_head(head);
            if (r.u8(body))
                out->set_look_mount_body(body);
            if (r.u8(legs))
                out->set_look_mount_legs(legs);
            if (r.u8(feet))
                out->set_look_mount_feet(feet);
        }

        uint8_t direction = 0, visible = 0;
        if (r.u8(direction))
            out->set_podium_direction(direction);
        if (r.u8(visible))
            out->set_podium_visible(visible != 0);
    }

    if (g_game.getFeature(tibia::client::GameThingUpgradeClassification) && type->getClassification() > 0) {
        uint8_t tier = 0;
        if (r.u8(tier))
            out->set_tier(tier);
    }

    if (g_game.getFeature(tibia::client::GameThingClock)
        && (type->hasClockExpire() || type->hasExpire() || type->hasExpireStop())) {
        uint32_t duration = 0;
        uint8_t brandNew = 0;
        if (r.u32(duration))
            out->set_duration_seconds(duration);
        if (r.u8(brandNew))
            out->set_brand_new(brandNew != 0);
    }

    if (g_game.getFeature(tibia::client::GameThingCounter) && type->hasWearOut()) {
        uint32_t charges = 0;
        uint8_t brandNew = 0;
        if (r.u32(charges))
            out->set_charges(charges);
        if (r.u8(brandNew))
            out->set_brand_new(brandNew != 0);
    }

    if (g_game.getFeature(tibia::client::GameWrapKit) && type->isDecoKit()) {
        uint16_t deco = 0;
        if (r.u16(deco))
            out->set_deco_item_id(deco);
    }

    if (g_game.getFeature(tibia::client::GameItemShader)) {
        std::string shader;
        if (r.string(shader))
            out->set_shader(shader);
    }
    if (g_game.getFeature(tibia::client::GameItemTooltipV8)) {
        std::string tooltip;
        if (r.string(tooltip))
            out->set_tooltip(tooltip);
    }

    return true;
}

void writeInventoryItem(ClassicWriter& w, const ProtocolInventoryItem& item)
{
    const uint16_t id = static_cast<uint16_t>(item.item_id());
    w.u16(id);

    ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return;

    if (g_game.getClientVersion() < 1281 && g_game.getFeature(tibia::client::GameThingMarks))
        w.u8(uint8_t(item.has_mark() ? item.mark() : 0));

    if (type->isStackable() || type->isFluidContainer() || type->isSplash() || type->isChargeable()) {
        const uint16_t count = static_cast<uint16_t>(item.has_count_or_subtype() ? item.count_or_subtype() : 0);
        if (g_game.getFeature(tibia::client::GameCountU16))
            w.u16(count);
        else
            w.u8(uint8_t(count));
    }

    if (g_game.getFeature(tibia::client::GameItemAnimationPhase) && type->getAnimationPhases() > 1)
        w.u8(uint8_t(item.has_animation_phase() ? item.animation_phase() : 0));

    if (type->isContainer()) {
        if (g_game.getFeature(tibia::client::GameContainerTypes)) {
            const uint8_t containerType = uint8_t(item.has_container_type() ? item.container_type() : 0);
            w.u8(containerType);
            switch (containerType) {
            case 1:
                w.u32(item.has_loot_flags() ? item.loot_flags() : 0);
                break;
            case 2:
                w.u32(item.has_container_ammo_total() ? item.container_ammo_total() : 0);
                break;
            case 3:
                w.u32(item.has_loot_flags() ? item.loot_flags() : 0);
                w.u32(item.has_obtain_flags() ? item.obtain_flags() : 0);
                break;
            case 4:
                break;
            case 8:
                w.u32(item.has_obtain_flags() ? item.obtain_flags() : 0);
                break;
            case 9:
                w.u32(item.has_loot_flags() ? item.loot_flags() : 0);
                if (g_game.getClientVersion() >= 1332)
                    w.u32(item.has_obtain_flags() ? item.obtain_flags() : 0);
                break;
            case 11:
                w.u32(item.has_loot_flags() ? item.loot_flags() : 0);
                w.u32(item.has_container_ammo_total() ? item.container_ammo_total() : 0);
                if (g_game.getClientVersion() >= 1332)
                    w.u32(item.has_obtain_flags() ? item.obtain_flags() : 0);
                break;
            default:
                break;
            }
        } else {
            if (g_game.getFeature(tibia::client::GameThingQuickLoot)) {
                const bool hasLoot = item.has_loot_flags();
                w.u8(hasLoot ? 1 : 0);
                if (hasLoot)
                    w.u32(item.loot_flags());
            }
            if (g_game.getFeature(tibia::client::GameThingQuiver)) {
                const bool hasAmmo = item.has_has_container_ammo() && item.has_container_ammo();
                w.u8(hasAmmo ? 1 : 0);
                if (hasAmmo)
                    w.u32(item.has_container_ammo_total() ? item.container_ammo_total() : 0);
            }
        }
    }

    if (g_game.getFeature(tibia::client::GameThingPodium) && type->isPodium()) {
        const uint16_t looktype = static_cast<uint16_t>(item.has_look_type() ? item.look_type() : 0);
        w.u16(looktype);
        if (looktype != 0) {
            w.u8(uint8_t(item.has_look_head() ? item.look_head() : 0));
            w.u8(uint8_t(item.has_look_body() ? item.look_body() : 0));
            w.u8(uint8_t(item.has_look_legs() ? item.look_legs() : 0));
            w.u8(uint8_t(item.has_look_feet() ? item.look_feet() : 0));
            w.u8(uint8_t(item.has_look_addons() ? item.look_addons() : 0));
        } else if (g_game.getFeature(tibia::client::GameThingPodiumItemType)) {
            w.u16(uint16_t(item.has_look_type_ex() ? item.look_type_ex() : 0));
        }

        const uint16_t lookmount = static_cast<uint16_t>(item.has_look_mount() ? item.look_mount() : 0);
        w.u16(lookmount);
        if (lookmount != 0) {
            w.u8(uint8_t(item.has_look_mount_head() ? item.look_mount_head() : 0));
            w.u8(uint8_t(item.has_look_mount_body() ? item.look_mount_body() : 0));
            w.u8(uint8_t(item.has_look_mount_legs() ? item.look_mount_legs() : 0));
            w.u8(uint8_t(item.has_look_mount_feet() ? item.look_mount_feet() : 0));
        }
        w.u8(uint8_t(item.has_podium_direction() ? item.podium_direction() : 0));
        w.u8(item.has_podium_visible() && item.podium_visible() ? 1 : 0);
    }

    if (g_game.getFeature(tibia::client::GameThingUpgradeClassification) && type->getClassification() > 0)
        w.u8(uint8_t(item.has_tier() ? item.tier() : 0));

    if (g_game.getFeature(tibia::client::GameThingClock)
        && (type->hasClockExpire() || type->hasExpire() || type->hasExpireStop())) {
        w.u32(item.has_duration_seconds() ? item.duration_seconds() : 0);
        w.u8(item.has_brand_new() && item.brand_new() ? 1 : 0);
    }

    if (g_game.getFeature(tibia::client::GameThingCounter) && type->hasWearOut()) {
        w.u32(item.has_charges() ? item.charges() : 0);
        w.u8(item.has_brand_new() && item.brand_new() ? 1 : 0);
    }

    if (g_game.getFeature(tibia::client::GameWrapKit) && type->isDecoKit())
        w.u16(uint16_t(item.has_deco_item_id() ? item.deco_item_id() : 0));

    if (g_game.getFeature(tibia::client::GameItemShader))
        w.string(item.has_shader() ? item.shader() : std::string());
    if (g_game.getFeature(tibia::client::GameItemTooltipV8))
        w.string(item.has_tooltip() ? item.tooltip() : std::string());
}

} // namespace tibia::protocol
