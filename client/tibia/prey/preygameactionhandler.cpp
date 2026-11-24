#include "prey/preygameactionhandler.h"

#include "input/gameactionhandlers.h"
#include "prey/preyprotocolmessagehandler.h"

#include <utility>

namespace tibia::prey {

using input::EGameAction;

TPreyGameActionHandler::TPreyGameActionHandler(std::shared_ptr<TPreyProtocolMessageHandler> protocolMessageHandler)
    : m_protocolMessageHandler(std::move(protocolMessageHandler))
{
}

void TPreyGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case EGameAction::PREY_REQUEST_LIST_REROLL:
        if (const auto* action = input::gameActionAs<input::TGameActionPreySystem>(pGameAction))
            handleRequestListRerollGameAction(*action);
        return;
    case EGameAction::PREY_SELECT_MONSTER_FROM_LIST:
        if (const auto* action = input::gameActionAs<input::TGameActionSelectPreyMonsterFromList>(pGameAction))
            handleSelectMonsterFromListGameAction(*action);
        return;
    case EGameAction::PREY_REQUEST_BONUS_REROLL:
        if (const auto* action = input::gameActionAs<input::TGameActionPreySystem>(pGameAction))
            handleRequestBonusRerollGameAction(*action);
        return;
    case EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE:
        if (const auto* action = input::gameActionAs<input::TGameActionChangeAutoPreyExtendType>(pGameAction))
            handleRequestChangeAutoPreyExtendTypeGameAction(*action);
        return;
    case EGameAction::PREY_REQUEST_FREEZE_AND_SHOW_BESTIARY_MONSTERS:
        if (const auto* action = input::gameActionAs<input::TGameActionPreySystem>(pGameAction))
            handleRequestFreezeAndShowBestiaryMonstersGameAction(*action);
        return;
    case EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY:
        if (const auto* action = input::gameActionAs<input::TGameActionSelectPreyMonsterFromBestiary>(pGameAction))
            handleSelectPreyMonsterFromBestiaryGameAction(*action);
        return;
    default:
        return;
    }
}

void TPreyGameActionHandler::handleRequestListRerollGameAction(const input::TGameActionPreySystem& gameAction)
{
    m_protocolMessageHandler->requestListReroll(gameAction.preyId());
}

void TPreyGameActionHandler::handleSelectMonsterFromListGameAction(const input::TGameActionSelectPreyMonsterFromList& gameAction)
{
    m_protocolMessageHandler->selectMonsterFromList(gameAction.preyId(), gameAction.listIndex());
}

void TPreyGameActionHandler::handleRequestBonusRerollGameAction(const input::TGameActionPreySystem& gameAction)
{
    m_protocolMessageHandler->requestBonusReroll(gameAction.preyId());
}

void TPreyGameActionHandler::handleRequestFreezeAndShowBestiaryMonstersGameAction(const input::TGameActionPreySystem& gameAction)
{
    m_protocolMessageHandler->requestFreezeAndShowBestiaryMonsters(gameAction.preyId());
}

void TPreyGameActionHandler::handleSelectPreyMonsterFromBestiaryGameAction(const input::TGameActionSelectPreyMonsterFromBestiary& gameAction)
{
    m_protocolMessageHandler->selectMonsterFromBestiary(gameAction.preyId(), gameAction.raceId());
}

void TPreyGameActionHandler::handleRequestChangeAutoPreyExtendTypeGameAction(const input::TGameActionChangeAutoPreyExtendType& gameAction)
{
    m_protocolMessageHandler->setAutomaticExtendType(gameAction.preyId(), gameAction.extendType());
}

}
