#pragma once

// CIP-named spells surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <cstdint>

namespace tibia::protobuf::protocol {
class GameserverMessageSpellDataModification;
class GameserverMessageUnlockedSpells;
}

namespace tibia {
namespace spells {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TSpellInfoStorage;

/// The protocol's spell modification type (0..3); other values throw.
uint32_t spellModificationType(uint32_t protocolValue);

/// The session's handler of the spell messages: the spell data modifications
/// go to the spell info storage, each unlocked spell is announced
/// (receivedUnlockedSpell, the action bars follow it).
class TSpellsProtocolMessageHandler final : public cip::tibia::spells::TSpellsProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSpellsProtocolMessageHandler(TSpellInfoStorage* spellInfoStorage);

    /// The server's spell data modifications, grouped by spell, replace the
    /// storage's whole set.
    void handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message);
    /// receivedUnlockedSpell for every spell of the message, in its order.
    void handleUnlockedSpellsMessage(const ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells& message);

private:
    TSpellInfoStorage* m_spellInfoStorage = nullptr;
};

}
}

