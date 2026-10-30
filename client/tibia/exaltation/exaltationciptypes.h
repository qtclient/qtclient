#pragma once

// CIP-named exaltation surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::exaltation {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TExaltationGameActionHandler final : public cip::tibia::exaltation::TExaltationGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::exaltation::TExaltationGameActionHandler::TExaltationGameActionHandler;
};

class TExaltationProtocolMessageHandler final : public cip::tibia::exaltation::TExaltationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::exaltation::TExaltationProtocolMessageHandler::TExaltationProtocolMessageHandler;
};

} // namespace tibia::exaltation

