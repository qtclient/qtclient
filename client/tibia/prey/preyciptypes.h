#pragma once

// CIP-named prey surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::prey {

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
};

} // namespace tibia::prey

