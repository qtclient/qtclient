#pragma once

// Classic wire → protobuf for the world map messages: the fields of an area
// with their objects and creatures, read in the server's order.

#include "protocol/protocolclassicwire.h"

#include <google/protobuf/repeated_ptr_field.h>

namespace tibia::protobuf::protocol {
class GameserverMessage;
class ProtocolCreature;
class ProtocolMapField;
class ProtocolMapObject;
class ProtocolOutfit;
}

namespace tibia::protocol {

/// Outfit (`withAddons`: colours + addons, or an item when the look type is 0) or mount.
bool readOutfit(ClassicReader& r, protobuf::protocol::ProtocolOutfit* out, bool outfit);
/// A creature after its u16 kind: new (0x61), known (0x62) or turned (0x63).
bool readCreature(ClassicReader& r, protobuf::protocol::ProtocolCreature* out, uint16_t kind);
/// An object or a creature (u16 id first).
bool readMapObject(ClassicReader& r, protobuf::protocol::ProtocolMapObject* out);
/// A field's objects up to its end marker; `skip` receives the fields to skip after it.
bool readMapField(ClassicReader& r, protobuf::protocol::ProtocolMapField* out, int& skip);

/// The fields of one floor, column by column, carrying `skip` over (the
/// classic floor description; `offset` shifts towards the view's floor).
bool readMapFloor(ClassicReader& r, google::protobuf::RepeatedPtrField<protobuf::protocol::ProtocolMapField>* fields, int x,
                  int y, int z, int width, int height, int offset, int& skip);

/// Fill a world map message (FullMap, the four slices, TopFloor, BottomFloor,
/// FieldData, CreateOnMap, ChangeOnMap, DeleteOnMap); 0 when the body is short.
size_t fillMapMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
/// Move the translator's view after a world map message was translated.
void advanceMapViewport(const protobuf::protocol::GameserverMessage& msg);
bool isMapMessage(int opcode);
/// Fill a creature message (CreatureUpdate, CreatureOutfit, CreatureMarks,
/// CreatureType, SnapBack, Wait); 0 when the body is short.
size_t fillCreatureMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isCreatureMessage(int opcode);
/// Centre the translator's view (a floor description re-centring the map).
void setMapViewport(int x, int y, int z);

}
