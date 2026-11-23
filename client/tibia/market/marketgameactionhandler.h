#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia::market {

class TMarketProtocolMessageHandler;

/// CIP: TMarketGameActionHandler. Turns the MARKET_BROWSE game action (the
/// object context menus' "Show in Market") into the market protocol's item
/// browse request.
class TMarketGameActionHandler final : public cip::tibia::market::TMarketGameActionHandler
{
    Q_OBJECT
public:
    explicit TMarketGameActionHandler(std::shared_ptr<TMarketProtocolMessageHandler> marketProtocolMessageHandler);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void handleMarketBrowseGameAction(const ::tibia::input::gameactions::TGameActionMarketBrowse& pGameActionMarketBrowse);

private:
    std::shared_ptr<TMarketProtocolMessageHandler> m_marketProtocolMessageHandler;
};

}
