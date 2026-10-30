#pragma once

// CIP-named exivaoptions surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::exivaoptions {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TExivaOptionsGameActionHandler final : public cip::tibia::exivaoptions::TExivaOptionsGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::exivaoptions::TExivaOptionsGameActionHandler::TExivaOptionsGameActionHandler;
};

class TExivaOptionsProtocolMessageHandler final : public cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler::TExivaOptionsProtocolMessageHandler;
};

} // namespace tibia::exivaoptions

