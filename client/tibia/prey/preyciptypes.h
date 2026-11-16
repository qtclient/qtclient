#pragma once

// CIP-named prey surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessagePreyData;
class GameserverMessagePreyPrices;
class GameserverMessagePreyTimeLeft;
}

namespace tibia {
namespace prey {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TPreyGameActionHandler final : public cip::tibia::prey::TPreyGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::prey::TPreyGameActionHandler::TPreyGameActionHandler;
};

class TPreyProtocolMessageHandler final : public cip::tibia::prey::TPreyProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::prey::TPreyProtocolMessageHandler::TPreyProtocolMessageHandler;
public:

    static void handlePreyTimeLeftMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyTimeLeft& message);
    /// One slot's state; every state also carries the next free reroll and the option.
    static void handlePreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& message);
    static void handlePreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& message);
};

}
}

