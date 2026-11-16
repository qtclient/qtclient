#pragma once

// CIP-named spells surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageSpellDataModification;
}

namespace tibia {
namespace spells {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TSpellsProtocolMessageHandler final : public cip::tibia::spells::TSpellsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::spells::TSpellsProtocolMessageHandler::TSpellsProtocolMessageHandler;
    /// The server's spell data modifications, grouped by spell, replace the
    /// storage's whole set.
    static void handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message);
};

}
}

