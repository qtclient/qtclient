#include "proficiency/proficiencyciptypes.h"
#include <vector>
#include <string>
#include <map>
#include <tibia/game/thingsession_forward.h>
#include <tibia/game/gamesession.h>
#include <tibia/assets/thingtype.h>
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::proficiency {

namespace {
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

void TProficiencyProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 187:
        if (shadow->HasExtension(GameserverMessageShapedPerkReshapeOffers::gameserver_message_shaped_perk_reshape_offers)) {
            const auto& m = shadow->GetExtension(GameserverMessageShapedPerkReshapeOffers::gameserver_message_shaped_perk_reshape_offers);
            std::vector<std::map<std::string, uint16_t>> offers;
            for (const auto& offer : m.slot().offers())
                offers.push_back({ { "modifierEnum", static_cast<uint16_t>(offer.modifier()) },
                                   { "refineLevel", static_cast<uint16_t>(offer.refine_level()) } });
            tibia::game::notify("onWeaponProficiencyReshape", static_cast<uint16_t>(m.object_id()), static_cast<uint8_t>(m.slot().level()),
                                static_cast<uint8_t>(m.slot().position()), offers);
        }
        break;
    case 196:
        if (shadow->HasExtension(GameserverMessageWeaponProficiency::gameserver_message_weapon_proficiency)) {
            const auto& m = shadow->GetExtension(GameserverMessageWeaponProficiency::gameserver_message_weapon_proficiency);
            const auto itemId = static_cast<uint16_t>(m.object_id());
            std::map<uint8_t, uint8_t> perks;
            for (const auto& perk : m.perks())
                perks[static_cast<uint8_t>(perk.level())] = static_cast<uint8_t>(perk.position());
            std::vector<std::map<std::string, uint16_t>> modified;
            for (const auto& perk : m.modified_perks())
                modified.push_back({ { "grade", static_cast<uint16_t>(perk.level()) },
                                     { "slot", static_cast<uint16_t>(perk.position()) },
                                     { "modifierEnum", static_cast<uint16_t>(perk.modifier()) },
                                     { "refineLevel", static_cast<uint16_t>(perk.refine_level()) } });
            // The market category of the weapon (all weapons when it has none).
            constexpr uint16_t MarketCategoryWeaponsAll = 32;
            uint16_t marketCategory = MarketCategoryWeaponsAll;
            if (tibia::game::isValidDatId(itemId, ThingCategoryItem)) {
                if (const auto& type = tibia::game::thingType(itemId, ThingCategoryItem); type && !type->getMarketData().name.empty())
                    marketCategory = type->getMarketData().category;
            }
            tibia::game::notify("onWeaponProficiency", itemId, m.experience(), perks, marketCategory, modified);
        }
        break;
    case 92:
        if (shadow->HasExtension(GameserverMessageWeaponProficiencyNotification::gameserver_message_weapon_proficiency_notification)) {
            const auto& m = shadow->GetExtension(GameserverMessageWeaponProficiencyNotification::gameserver_message_weapon_proficiency_notification);
            tibia::game::notify("onWeaponProficiencyExperience", static_cast<uint16_t>(m.object_id()), m.experience(),
                                m.unused_perk());
        }
        break;
    default:
        break;
    }
}

}
