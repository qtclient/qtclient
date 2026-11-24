#include "exaltation/exaltationgameactionhandler.h"

#include "client/const.h"
#include "exaltation/exaltationprotocolmessagehandler.h"
#include "exaltation/exaltationstorage.h"

#include <utility>

namespace tibia::exaltation {

using A = input::EGameAction;

TExaltationGameActionHandler::TExaltationGameActionHandler(std::shared_ptr<TExaltationProtocolMessageHandler> protocolMessageHandler,
                                                           std::shared_ptr<TExaltationStorage> exaltationStorage)
    : m_protocolMessageHandler(std::move(protocolMessageHandler))
    , m_exaltationStorage(std::move(exaltationStorage))
{
}

bool TExaltationGameActionHandler::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::EXALTATION_FORGE_DIALOG_CLOSED: handleExaltationForgeDialogClosedGameAction(); break;
    case A::EXALTATION_FORGE_FUSION:
        handleExaltationForgeFusionGameAction(static_cast<const input::TGameActionExaltationForgeFusion&>(action));
        break;
    case A::EXALTATION_FORGE_CONVERGENCE_FUSION:
        handleExaltationForgeConvergenceFusionGameAction(static_cast<const input::TGameActionExaltationForgeConvergenceFusion&>(action));
        break;
    case A::EXALTATION_FORGE_TRANSFER:
        handleExaltationForgeTransferGameAction(static_cast<const input::TGameActionExaltationForgeTransfer&>(action));
        break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE: m_protocolMessageHandler->requestHistoryPage(0); break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE: handleExaltationForgeHistoryNextPageGameAction(); break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE: handleExaltationForgeHistoryPreviousPageGameAction(); break;
    default: return false;
    }
    return true;
}

void TExaltationGameActionHandler::handleExaltationForgeDialogClosedGameAction()
{
    m_exaltationStorage->reset();
}

void TExaltationGameActionHandler::handleExaltationForgeFusionGameAction(const input::TGameActionExaltationForgeFusion& action)
{
    m_protocolMessageHandler->requestFusion(action.typeId(), action.tier(), action.resourceTypeId(), action.successRateIncreased(),
                                            action.tierLossReduced());
}

void TExaltationGameActionHandler::handleExaltationForgeConvergenceFusionGameAction(
    const input::TGameActionExaltationForgeConvergenceFusion& action)
{
    m_protocolMessageHandler->requestConvergenceFusion(action.typeId(), action.tier(), action.resourceTypeId());
}

void TExaltationGameActionHandler::handleExaltationForgeTransferGameAction(const input::TGameActionExaltationForgeTransfer& action)
{
    m_protocolMessageHandler->requestTransfer(action.sourceTypeId(), action.tier(), action.targetTypeId(), action.convergence());
}

void TExaltationGameActionHandler::handleExaltationForgeHistoryNextPageGameAction()
{
    const auto next = static_cast<uint16_t>(m_exaltationStorage->currentPage() + 1);
    if (next < m_exaltationStorage->numberOfPages())
        m_protocolMessageHandler->requestHistoryPage(next);
}

void TExaltationGameActionHandler::handleExaltationForgeHistoryPreviousPageGameAction()
{
    if (const uint16_t page = m_exaltationStorage->currentPage(); page != 0)
        m_protocolMessageHandler->requestHistoryPage(page - 1);
}

}
