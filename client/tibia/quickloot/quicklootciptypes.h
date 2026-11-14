#pragma once

// CIP-named quickloot surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace quickloot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TManagedContainersProtocolMessageHandler final : public cip::tibia::quickloot::TManagedContainersProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TManagedContainersProtocolMessageHandler::TManagedContainersProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
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

}
}

