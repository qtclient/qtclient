#include "store/storegameactionhandler.h"

#include <utility>

namespace tibia::store {

using input::EGameAction;

TStoreOpenParameters openParametersFromGameAction(const input::TStoreOpenParameters& action)
{
    TStoreOpenParameters parameters;
    switch (action.openAction) {
        case OPEN_PREMIUM_OR_BOOSTS:
            parameters.openAction = action.openAction;
            parameters.premiumDetail = action.detail;
            break;
        case OPEN_CATEGORY:
            parameters.openAction = action.text.isEmpty() ? OPEN_HOME : action.openAction;
            parameters.category = action.text;
            break;
        case OPEN_USEFUL_THING:
            parameters.openAction = action.openAction;
            parameters.usefulThing = action.detail;
            break;
        case OPEN_OFFER:
            parameters.openAction = action.openAction;
            parameters.offerId = static_cast<TOfferID>(action.detail);
            break;
        case OPEN_SEARCH:
            parameters.openAction = action.text.isEmpty() ? OPEN_HOME : action.openAction;
            parameters.category = action.text;
            break;
        default: parameters.openAction = OPEN_HOME; break;
    }
    return parameters;
}

TStoreGameActionHandler::TStoreGameActionHandler(std::shared_ptr<TStoreProtocolMessageHandler> storeProtocolMessageHandler)
    : cip::tibia::store::TStoreGameActionHandler(nullptr)
    , m_storeProtocolMessageHandler(std::move(storeProtocolMessageHandler))
{
}

void TStoreGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || !m_storeProtocolMessageHandler)
        return;
    switch (pGameAction->gameActionType()) {
    case EGameAction::REQUEST_TRANSACTION_DETAILS:
        if (const auto* action = input::gameActionAs<input::TGameActionRequestTransactionDetailsFromServer>(pGameAction))
            handleRequestTransactionDetailsGameAction(*action);
        return;
    case EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE:
        if (const auto* action = input::gameActionAs<input::TGameActionBuyHirelingNameOrSexChange>(pGameAction))
            handleBuyHirelingNameOrSexChangeAction(*action);
        return;
    case EGameAction::OPEN_STORE:
        if (const auto* action = input::gameActionAs<input::TGameActionOpenStore>(pGameAction))
            handleOpenStoreGameAction(*action);
        return;
    default:
        return;
    }
}

void TStoreGameActionHandler::handleOpenStoreGameAction(const input::TGameActionOpenStore& pGameAction)
{
    if (m_storeProtocolMessageHandler)
        m_storeProtocolMessageHandler->openStore(openParametersFromGameAction(pGameAction.storeOpenParameters()));
}

void TStoreGameActionHandler::handleBuyHirelingNameOrSexChangeAction(const input::TGameActionBuyHirelingNameOrSexChange& pGameAction)
{
    if (m_storeProtocolMessageHandler)
        m_storeProtocolMessageHandler->openStore(openParametersFromGameAction(pGameAction.storeOpenParameters()));
}

void TStoreGameActionHandler::handleRequestTransactionDetailsGameAction(
    const input::TGameActionRequestTransactionDetailsFromServer& pGameAction)
{
    if (m_storeProtocolMessageHandler)
        m_storeProtocolMessageHandler->requestTransactionDetails(static_cast<uint32_t>(pGameAction.transactionId()));
}

}
