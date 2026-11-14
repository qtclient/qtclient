#include "prey/preyciptypes.h"
#include <vector>
#include <string>
#include <client/outfit.h>
#include <client/const.h>
#include "creatures/creatureprotocolapply.h"
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::prey {

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

} // namespace tibia::prey

namespace tibia::prey {

void TPreyProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 232:
        if (shadow->HasExtension(GameserverMessagePreyData::gameserver_message_prey_data)) {
            const auto& m = shadow->GetExtension(GameserverMessagePreyData::gameserver_message_prey_data);
            const auto slot = static_cast<uint8_t>(m.slot());
            const uint32_t reroll = m.next_free_reroll();
            // The tail byte is the option toggle of an active prey, the wildcards otherwise.
            const auto tail = static_cast<uint8_t>(m.option());
            const auto bonusType = static_cast<uint8_t>(m.bonus_type());
            const auto bonusValue = static_cast<uint16_t>(m.bonus_value());
            const auto bonusGrade = static_cast<uint8_t>(m.bonus_grade());
            std::vector<std::string> names;
            std::vector<Outfit> outfits;
            for (const auto& monster : m.monsters()) {
                names.push_back(monster.name());
                outfits.push_back(creatures::outfitFromProtocol(monster.outfit(), nullptr));
            }
            std::vector<uint16_t> races;
            for (const uint32_t race : m.race_ids())
                races.push_back(static_cast<uint16_t>(race));
            switch (m.state()) {
            case tibia::client::PREY_STATE_LOCKED:
                tibia::game::notify("onPreyLocked", slot, static_cast<tibia::client::PreyUnlockState_t>(m.unlock_state()), reroll, tail);
                break;
            case tibia::client::PREY_STATE_INACTIVE:
                tibia::game::notify("onPreyInactive", slot, reroll, tail);
                break;
            case tibia::client::PREY_STATE_ACTIVE:
                if (!names.empty())
                    tibia::game::notify("onPreyActive", slot, names.front(), outfits.front(), bonusType, bonusValue, bonusGrade,
                                        static_cast<uint16_t>(m.time_left()), reroll, static_cast<uint8_t>(0), tail);
                break;
            case tibia::client::PREY_STATE_SELECTION:
                tibia::game::notify("onPreySelection", slot, names, outfits, reroll, tail);
                break;
            case tibia::client::PREY_STATE_SELECTION_CHANGE_MONSTER:
                tibia::game::notify("onPreySelectionChangeMonster", slot, names, outfits, bonusType, bonusValue, bonusGrade, reroll, tail);
                break;
            case tibia::client::PREY_STATE_LIST_SELECTION:
                tibia::game::notify("onPreyListSelection", slot, races, reroll, tail);
                break;
            case tibia::client::PREY_STATE_WILDCARD_SELECTION:
                tibia::game::notify("onPreyWildcardSelection", slot, races, reroll, tail, bonusType, bonusValue, bonusGrade);
                break;
            default:
                break;
            }
        }
        break;
    case 231:
        if (shadow->HasExtension(GameserverMessagePreyTimeLeft::gameserver_message_prey_time_left)) {
            const auto& m = shadow->GetExtension(GameserverMessagePreyTimeLeft::gameserver_message_prey_time_left);
            tibia::game::notify("onPreyTimeLeft", static_cast<uint8_t>(m.slot()), static_cast<uint16_t>(m.time_left()));
        }
        break;
    case 233:
        if (shadow->HasExtension(GameserverMessagePreyPrices::gameserver_message_prey_prices)) {
            const auto& m = shadow->GetExtension(GameserverMessagePreyPrices::gameserver_message_prey_prices);
            tibia::game::notify("onPreyRerollPrice", m.reroll_price(), static_cast<uint8_t>(m.wildcard_price()),
                                static_cast<uint8_t>(m.selection_price()));
        }
        break;
    default:
        break;
    }
}

}
