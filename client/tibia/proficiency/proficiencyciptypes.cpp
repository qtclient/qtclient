#include "proficiency/proficiencyciptypes.h"
#include <vector>
#include <string>
#include <map>
#include <tibia/game/thingsession_forward.h>
#include <tibia/game/gamesession.h>
#include <tibia/appearances/appearancetype.h>
#include "protocol/protocolciptypes.h"
#include "proficiency/proficiencystorage.h"
#include "container/containerciptypes.h"
#include <client/const.h>
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::proficiency {

namespace {

/// The resource the proficiency storage follows: lunar ascension orbs.
constexpr uint32_t LunarAscensionOrbsResource = 73;

} // namespace

} // namespace tibia::proficiency

namespace tibia::proficiency {

TProficiencyProtocolMessageHandler::TProficiencyProtocolMessageHandler(TProficiencyStorage* storage,
                                                                       std::shared_ptr<container::TInventoryContainer> inventory)
    : cip::tibia::proficiency::TProficiencyProtocolMessageHandler(nullptr)
    , m_storage(storage)
    , m_inventory(std::move(inventory))
{
}

void TProficiencyProtocolMessageHandler::handleWeaponProficiencyMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiency& message)
{
    TProficiencyData data;
    data.picks.reserve(3);
    for (const auto& perk : message.perks())
        data.pick(static_cast<int>(perk.level()), static_cast<int>(perk.position()));
    data.shaped.reserve(static_cast<size_t>(message.modified_perks_size()));
    for (const auto& perk : message.modified_perks())
        data.shaped.push_back({ static_cast<uint8_t>(perk.level()), static_cast<uint8_t>(perk.position()),
                                static_cast<uint16_t>(perk.modifier()), static_cast<uint8_t>(perk.refine_level()) });
    data.xp = message.experience();
    m_storage->setProficiencyData(static_cast<uint16_t>(message.object_id()), std::move(data));
}

void TProficiencyProtocolMessageHandler::handleShapedPerkReshapeOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageShapedPerkReshapeOffers& message)
{
    TReshapeOffers offers;
    offers.typeId = static_cast<uint16_t>(message.object_id());
    offers.level = static_cast<uint8_t>(message.slot().level());
    offers.perk = static_cast<uint8_t>(message.slot().position());
    for (const auto& offer : message.slot().offers())
        offers.offers.push_back({ offers.level, offers.perk, static_cast<uint16_t>(offer.modifier()), static_cast<uint8_t>(offer.refine_level()) });
    m_storage->setReshapeOffers(std::move(offers));
}

void TProficiencyProtocolMessageHandler::handleWeaponProficiencyNotificationMessage(
    const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiencyNotification& message)
{
    // The wielded weapon is the left hand's object; an empty hand wields none.
    bool isCurrentWeapon = false;
    if (m_inventory) {
        const container::TInventorySlot& hand = m_inventory->slotAt(tibia::client::InventorySlotLeft);
        isCurrentWeapon = !hand.empty() && message.object_id() == hand.typeId;
    }
    m_storage->setProficiencyExperience(message.experience(), static_cast<uint16_t>(message.object_id()), isCurrentWeapon);
    if (isCurrentWeapon || message.object_id() == 0)
        m_storage->setUnspentPerk(message.unused_perk());
    // product: the pack follows proficiency XP gains.
    tibia::game::notify("onWeaponProficiencyExperience", static_cast<uint16_t>(message.object_id()), message.experience(),
                        message.unused_perk());
}

void TProficiencyProtocolMessageHandler::handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    if (message.resource_type() != LunarAscensionOrbsResource)
        return;
    m_storage->setLunarAscensionOrbs(static_cast<uint32_t>(message.value()));
}

}
