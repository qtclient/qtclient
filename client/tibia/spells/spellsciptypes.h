#pragma once

// CIP-named spells surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::spells {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TAimAtTargetConfigurationStorage final : public cip::tibia::spells::TAimAtTargetConfigurationStorage
{
    Q_OBJECT
public:
    using cip::tibia::spells::TAimAtTargetConfigurationStorage::TAimAtTargetConfigurationStorage;
};

class TSpellsProtocolMessageHandler final : public cip::tibia::spells::TSpellsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::spells::TSpellsProtocolMessageHandler::TSpellsProtocolMessageHandler;
};

} // namespace tibia::spells

