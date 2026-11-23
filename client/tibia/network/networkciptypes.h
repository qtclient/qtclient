#pragma once

// CIP-named network surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace network {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

class TNetworkQualityProtocolMessageHandler final : public cip::tibia::network::TNetworkQualityProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::network::TNetworkQualityProtocolMessageHandler::TNetworkQualityProtocolMessageHandler;
};

}
}
