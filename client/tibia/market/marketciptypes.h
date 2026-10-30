#pragma once

// CIP-named market surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::market {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TMarketGameActionHandler final : public cip::tibia::market::TMarketGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::market::TMarketGameActionHandler::TMarketGameActionHandler;
};

class TMarketProtocolMessageHandler final : public cip::tibia::market::TMarketProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::market::TMarketProtocolMessageHandler::TMarketProtocolMessageHandler;
};

} // namespace tibia::market

