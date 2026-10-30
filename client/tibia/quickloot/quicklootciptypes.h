#pragma once

// CIP-named quickloot surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::quickloot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TManagedContainersProtocolMessageHandler final : public cip::tibia::quickloot::TManagedContainersProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TManagedContainersProtocolMessageHandler::TManagedContainersProtocolMessageHandler;
};

class TQuickLootBlackWhitelistStorage final : public cip::tibia::quickloot::TQuickLootBlackWhitelistStorage
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TQuickLootBlackWhitelistStorage::TQuickLootBlackWhitelistStorage;
};

class TQuickLootGameActionHandler final : public cip::tibia::quickloot::TQuickLootGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TQuickLootGameActionHandler::TQuickLootGameActionHandler;
};

} // namespace tibia::quickloot

