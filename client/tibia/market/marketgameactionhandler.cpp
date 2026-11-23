#include "market/marketgameactionhandler.h"

#include "market/marketciptypes.h"

#include <optional>
#include <utility>

namespace tibia::market {

using input::EGameAction;

TMarketGameActionHandler::TMarketGameActionHandler(std::shared_ptr<TMarketProtocolMessageHandler> marketProtocolMessageHandler)
    : cip::tibia::market::TMarketGameActionHandler(nullptr)
    , m_marketProtocolMessageHandler(std::move(marketProtocolMessageHandler))
{
}

void TMarketGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != EGameAction::MARKET_BROWSE)
        return;
    if (const auto* action = input::gameActionAs<input::gameactions::TGameActionMarketBrowse>(pGameAction))
        handleMarketBrowseGameAction(*action);
}

void TMarketGameActionHandler::handleMarketBrowseGameAction(const input::gameactions::TGameActionMarketBrowse& pGameActionMarketBrowse)
{
    if (!m_marketProtocolMessageHandler)
        return;
    // Host: qtclient's action carries the tier as a plain byte (0 when the
    // type has no classification); the browse message sets the tier only for
    // classified types.
    m_marketProtocolMessageHandler->sendBrowseItem(pGameActionMarketBrowse.objectClientId(),
                                                   std::optional<uint8_t>(pGameActionMarketBrowse.tier()));
}

}
