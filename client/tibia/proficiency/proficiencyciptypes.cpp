#include "proficiency/proficiencyciptypes.h"
#include <vector>
#include <string>
#include <map>
#include <tibia/game/thingsession_forward.h>
#include <tibia/game/gamesession.h>
#include <tibia/assets/thingtype.h>
#include "protocol/protocolciptypes.h"
#include "proficiency/proficiencystorage.h"
#include <client/const.h>
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::proficiency {

namespace {

/// The resource the proficiency storage follows: lunar ascension orbs.
constexpr uint32_t LunarAscensionOrbsResource = 73;

/// A weapon's whole record: the XP, the perk picked on each level (the list
/// grows to the highest level) and the shaped perks.
void handleWeaponProficiencyMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiency& message)
{
    TProficiencyStorage* storage = TProficiencyStorage::instance();
    if (!storage)
        return;
    TProficiencyData data;
    data.picks.reserve(3);
    for (const auto& perk : message.perks())
        data.pick(static_cast<int>(perk.level()), static_cast<int>(perk.position()));
    data.shaped.reserve(static_cast<size_t>(message.modified_perks_size()));
    for (const auto& perk : message.modified_perks())
        data.shaped.push_back({ static_cast<uint8_t>(perk.level()), static_cast<uint8_t>(perk.position()),
                                static_cast<uint16_t>(perk.modifier()), static_cast<uint8_t>(perk.refine_level()) });
    data.xp = message.experience();
    storage->setProficiencyData(static_cast<uint16_t>(message.object_id()), std::move(data));
}

/// The perks a shaped slot can be reshaped into; every offer sits on the slot.
void handleShapedPerkReshapeOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageShapedPerkReshapeOffers& message)
{
    TProficiencyStorage* storage = TProficiencyStorage::instance();
    if (!storage)
        return;
    TReshapeOffers offers;
    offers.typeId = static_cast<uint16_t>(message.object_id());
    offers.level = static_cast<uint8_t>(message.slot().level());
    offers.perk = static_cast<uint8_t>(message.slot().position());
    for (const auto& offer : message.slot().offers())
        offers.offers.push_back({ offers.level, offers.perk, static_cast<uint16_t>(offer.modifier()), static_cast<uint8_t>(offer.refine_level()) });
    storage->setReshapeOffers(std::move(offers));
}

/// The XP of one weapon; the wielded weapon's also moves the current progress.
/// The unspent perk flag is only the wielded weapon's (or the notification's
/// without a weapon).
void handleWeaponProficiencyNotificationMessage(
    const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiencyNotification& message)
{
    TProficiencyStorage* storage = TProficiencyStorage::instance();
    if (!storage)
        return;
    const bool isCurrentWeapon = tibia::game::localPlayer()
        && message.object_id() == tibia::game::localPlayerInventoryTypeId(tibia::client::InventorySlotLeft);
    storage->setProficiencyExperience(message.experience(), static_cast<uint16_t>(message.object_id()), isCurrentWeapon);
    if (isCurrentWeapon || message.object_id() == 0)
        storage->setUnspentPerk(message.unused_perk());
    // product: the pack follows proficiency XP gains.
    tibia::game::notify("onWeaponProficiencyExperience", static_cast<uint16_t>(message.object_id()), message.experience(),
                        message.unused_perk());
}

void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    if (message.resource_type() != LunarAscensionOrbsResource)
        return;
    if (TProficiencyStorage* storage = TProficiencyStorage::instance())
        storage->setLunarAscensionOrbs(static_cast<uint32_t>(message.value()));
}

} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::proficiency

namespace tibia::proficiency {

void TProficiencyProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    switch (opcode) {
    case 187:
        if (shadow->HasExtension(GameserverMessageShapedPerkReshapeOffers::gameserver_message_shaped_perk_reshape_offers))
            handleShapedPerkReshapeOffersMessage(
                shadow->GetExtension(GameserverMessageShapedPerkReshapeOffers::gameserver_message_shaped_perk_reshape_offers));
        break;
    case 196:
        if (shadow->HasExtension(GameserverMessageWeaponProficiency::gameserver_message_weapon_proficiency))
            handleWeaponProficiencyMessage(shadow->GetExtension(GameserverMessageWeaponProficiency::gameserver_message_weapon_proficiency));
        break;
    case 92:
        if (shadow->HasExtension(GameserverMessageWeaponProficiencyNotification::gameserver_message_weapon_proficiency_notification))
            handleWeaponProficiencyNotificationMessage(
                shadow->GetExtension(GameserverMessageWeaponProficiencyNotification::gameserver_message_weapon_proficiency_notification));
        break;
    case 238:
        if (shadow->HasExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance))
            handleResourceBalanceMessage(shadow->GetExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance));
        break;
    default:
        break;
    }
}

}
