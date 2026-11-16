#pragma once

// CIP-named market surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageMarketBrowse;
class GameserverMessageMarketDetail;
class GameserverMessageMarketEnter;
}

namespace tibia {
namespace market {

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
    static void handleMarketDetailMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketDetail& MarketDetailMessage);
    static void handleMarketBrowseMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketBrowse& MarketBrowseMessage);
    static void handleMarketEnterMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketEnter& MarketEnterMessage);
};

}
}

