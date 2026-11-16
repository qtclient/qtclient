#pragma once

#include <cstdint>
#include <memory>

#include <QColor>
class Outfit;

namespace tibia::protobuf::protocol {
class ProtocolCreature;
class ProtocolOutfit;
}

namespace tibia::creatures {

class TCreature;
class TCreatureStorage;

/// An outfit from the protocol: a creature look with colours and addons, an
/// item, or the invisible effect when both are 0; `mount` adds the mount.
Outfit outfitFromProtocol(const protobuf::protocol::ProtocolOutfit& outfit, const protobuf::protocol::ProtocolOutfit* mount);
/// Apply a creature the server describes (new, known or turned on the map, or
/// one field of a known creature) to the creature storage and the local
/// player; returns its id, 0 when a known creature is not stored or the
/// message carries a value the client does not know.
uint32_t applyProtocolCreature(const protobuf::protocol::ProtocolCreature& creature);
/// Apply the fields the message carries to a stored creature; false when an
/// unknown vocation or icon abandoned the rest of the message (the fields
/// before it stay applied).
bool applyProtocolCreatureFields(TCreatureStorage& storage, const std::shared_ptr<TCreature>& creature,
                                 const protobuf::protocol::ProtocolCreature& message);
/// Host: re-sync the creature's appearance on the field stack it stands on.
void syncFieldStackFor(uint32_t creatureId);

}
