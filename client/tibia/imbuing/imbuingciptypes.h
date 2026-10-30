#pragma once

// CIP-named imbuing surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::imbuing {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TImbuementDurationsStorage final : public cip::tibia::imbuing::TImbuementDurationsStorage
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuementDurationsStorage::TImbuementDurationsStorage;
};

class TImbuingProtocolMessageHandler final : public cip::tibia::imbuing::TImbuingProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuingProtocolMessageHandler::TImbuingProtocolMessageHandler;
};

} // namespace tibia::imbuing

