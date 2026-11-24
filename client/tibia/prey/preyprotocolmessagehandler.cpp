#include "prey/preyprotocolmessagehandler.h"

#include "creatures/creatureprotocolapply.h"
#include "prey/preystorage.h"

#include <client/const.h>
#include <client/outfit.h>
#include <tibia/game/gamesession_forward.h>

#include <protocol.pb.h>

#include <stdexcept>
#include <utility>

namespace tibia::prey {

namespace {

void sendPreyAction(const int preyId, const uint8_t action, const int value)
{
    tibia::game::preyAction(static_cast<uint8_t>(preyId), action, static_cast<uint16_t>(value));
}

/// A monster is its name and its outfit.
TPreyMonster preyMonster(const ::tibia::protobuf::protocol::ProtocolPreyMonster& monster)
{
    const ::Outfit outfit = creatures::outfitFromProtocol(monster.outfit(), nullptr);
    return { .name = QString::fromStdString(monster.name()),
             .outfitId = outfit.getId(),
             .head = outfit.getHead(),
             .torso = outfit.getBody(),
             .legs = outfit.getLegs(),
             .detail = outfit.getFeet(),
             .firstAddOn = (outfit.getAddons() & 1) != 0,
             .secondAddOn = (outfit.getAddons() & 2) != 0 };
}

}

TPreyProtocolMessageHandler::TPreyProtocolMessageHandler(std::shared_ptr<TPreyStorage> preyStorage)
    : m_preyStorage(std::move(preyStorage))
{
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
    m_preyStorage->setSlotTimeLeft(static_cast<int>(message.slot()), static_cast<int>(message.time_left()));
}

void TPreyProtocolMessageHandler::handlePreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& m)
{
    const int preyId = static_cast<int>(m.slot());
    // Every state carries the next free list reroll (seconds from now) and the
    // automatic extension option.
    m_preyStorage->setSlotNextFreeListReroll(preyId, static_cast<qint64>(m.next_free_reroll()));
    m_preyStorage->setSlotAutomaticExtendType(preyId, static_cast<int>(static_cast<uint8_t>(m.option())));
    const TPreyBonus bonus{ .type = static_cast<int>(static_cast<uint8_t>(m.bonus_type())),
                            .value = static_cast<int>(static_cast<uint16_t>(m.bonus_value())),
                            .grade = static_cast<int>(static_cast<uint8_t>(m.bonus_grade())) };
    QList<TPreyMonster> monsters;
    for (const auto& monster : m.monsters())
        monsters.append(preyMonster(monster));
    QList<uint16_t> races;
    for (const uint32_t race : m.race_ids())
        races.append(static_cast<uint16_t>(race));
    switch (m.state()) {
    case tibia::client::PREY_STATE_LOCKED:
        m_preyStorage->setSlotLocked(preyId, static_cast<int>(static_cast<tibia::client::PreyUnlockState_t>(m.unlock_state())));
        break;
    case tibia::client::PREY_STATE_INACTIVE:
        m_preyStorage->setSlotInactive(preyId);
        break;
    case tibia::client::PREY_STATE_ACTIVE:
        // The active monster is the first one; the time left comes with it.
        if (!monsters.isEmpty())
            m_preyStorage->setSlotActive(preyId, bonus, monsters.front(), static_cast<int>(static_cast<uint16_t>(m.time_left())));
        break;
    case tibia::client::PREY_STATE_SELECTION:
        m_preyStorage->setSlotSelection(preyId, std::move(monsters));
        break;
    case tibia::client::PREY_STATE_SELECTION_CHANGE_MONSTER:
        m_preyStorage->setSlotSelectionChangeMonster(preyId, bonus, std::move(monsters));
        break;
    case tibia::client::PREY_STATE_LIST_SELECTION:
        m_preyStorage->setSlotListSelection(preyId, std::move(races));
        break;
    case tibia::client::PREY_STATE_WILDCARD_SELECTION:
        m_preyStorage->setSlotWildcardSelection(preyId, bonus, std::move(races));
        break;
    default:
        break;
    }
}

void TPreyProtocolMessageHandler::handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message)
{
    if ((message.dialog_type() & ~1u) == 0x14)
        Q_EMIT requestShowMessageDialog(QString::fromStdString(message.message()));
}

void TPreyProtocolMessageHandler::handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    if (message.resource_type() != tibia::client::RESOURCE_PREY_WILDCARDS)
        return;
    m_preyStorage->setPreyWildcards(static_cast<int>(message.value()));
}

void TPreyProtocolMessageHandler::handlePreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& message)
{
    m_preyStorage->setPrices(static_cast<int>(message.reroll_price()), static_cast<int>(static_cast<uint8_t>(message.wildcard_price())),
                             static_cast<int>(static_cast<uint8_t>(message.selection_price())));
}

}
