#include "prey/preyciptypes.h"
#include <vector>
#include <string>
#include <client/outfit.h>
#include <client/const.h>
#include "creatures/creatureprotocolapply.h"
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>
#include <tibia/game/gamesession_forward.h>

#include <stdexcept>

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

namespace {

void sendPreyAction(const int preyId, const uint8_t action, const int value)
{
    tibia::game::preyAction(static_cast<uint8_t>(preyId), action, static_cast<uint16_t>(value));
}

}

void TPreyProtocolMessageHandler::requestListReroll(const int preyId)
{
    sendPreyAction(preyId, 0, 0);
}

void TPreyProtocolMessageHandler::requestBonusReroll(const int preyId)
{
    sendPreyAction(preyId, 1, 0);
}

void TPreyProtocolMessageHandler::selectMonsterFromList(const int preyId, const int index)
{
    sendPreyAction(preyId, 2, index);
}

void TPreyProtocolMessageHandler::requestFreezeAndShowBestiaryMonsters(const int preyId)
{
    sendPreyAction(preyId, 3, 0);
}

void TPreyProtocolMessageHandler::selectMonsterFromBestiary(const int preyId, const int raceId)
{
    sendPreyAction(preyId, 4, raceId);
}

void TPreyProtocolMessageHandler::setAutomaticExtendType(const int preyId, const int option)
{
    if (static_cast<unsigned>(option) >= 3)
        throw std::invalid_argument("Invalid enum value");
    sendPreyAction(preyId, 5, option);
}

void TPreyProtocolMessageHandler::handlePreyTimeLeftMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyTimeLeft& message)
{
    tibia::game::notify("onPreyTimeLeft", static_cast<uint8_t>(message.slot()), static_cast<uint16_t>(message.time_left()));
}

void TPreyProtocolMessageHandler::handlePreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& m)
{
    const auto slot = static_cast<uint8_t>(m.slot());
    const uint32_t reroll = m.next_free_reroll();
    // The tail byte is the option toggle of an active prey, the wildcards otherwise.
    const auto tail = static_cast<uint8_t>(m.option());
    const auto bonusType = static_cast<uint8_t>(m.bonus_type());
    const auto bonusValue = static_cast<uint16_t>(m.bonus_value());
    const auto bonusGrade = static_cast<uint8_t>(m.bonus_grade());
    // A monster is its name and its outfit.
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
        // The active monster is the first one; the time left comes with it.
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

void TPreyProtocolMessageHandler::handlePreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& message)
{
    tibia::game::notify("onPreyRerollPrice", message.reroll_price(), static_cast<uint8_t>(message.wildcard_price()),
                        static_cast<uint8_t>(message.selection_price()));
}

}
