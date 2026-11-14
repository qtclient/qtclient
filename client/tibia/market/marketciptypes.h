#pragma once

// CIP-named market surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

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

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

}
}

