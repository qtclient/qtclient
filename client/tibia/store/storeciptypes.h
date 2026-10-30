#pragma once

// CIP-named store surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::store {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TStoreDescriptionManager final : public cip::tibia::store::TStoreDescriptionManager
{
    Q_OBJECT
public:
    using cip::tibia::store::TStoreDescriptionManager::TStoreDescriptionManager;
};

class TStoreGameActionHandler final : public cip::tibia::store::TStoreGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::store::TStoreGameActionHandler::TStoreGameActionHandler;
};

} // namespace tibia::store
