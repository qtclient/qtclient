#pragma once

#include <cstdint>

class Outfit;

namespace tibia::protobuf::protocol {
class ProtocolCreature;
class ProtocolOutfit;
}

namespace tibia::creatures {

/// An outfit from the protocol: a creature look with colours and addons, an
/// item, or the invisible effect when both are 0; `mount` adds the mount.
Outfit outfitFromProtocol(const protobuf::protocol::ProtocolOutfit& outfit, const protobuf::protocol::ProtocolOutfit* mount);
/// Apply a creature the server describes on the map (new, known or turned) to
/// the creature storage and the local player; returns its id.
uint32_t applyProtocolCreature(const protobuf::protocol::ProtocolCreature& creature);

}
