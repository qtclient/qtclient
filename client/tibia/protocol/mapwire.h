#pragma once

// Classic wire → protobuf for the world map messages (CIP:
// TProtobufServerMessageTranslator's map, object and creature readers): the
// fields of an area with their objects and creatures, read in the server's
// order into CipSoft's map message layout.

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class AppearanceInstance;
class CreatureData;
class GameserverMessage;
class MapField;
class Outfit;
class ProtocolOutfit;
}

namespace tibia::protocol {

/// Outfit (`withAddons`: colours + addons, or an item when the look type is 0) or mount.
bool readOutfit(ClassicReader& r, protobuf::protocol::ProtocolOutfit* out, bool outfit);
/// CIP's outfit reader: u16 look type; non-zero -> four colours (`colors`; left
/// out when all are zero) and u8 addons (`addons`; left out when zero); zero ->
/// u16 object type (`object`).
bool readOutfit(ClassicReader& r, protobuf::protocol::Outfit* out, bool colors, bool addons, bool object);
/// A creature after its u16 kind: new (0x61), known (0x62) or turned (0x63).
bool readCreature(ClassicReader& r, protobuf::protocol::CreatureData* out, uint16_t kind);
/// An object or a creature (u16 id first unless `id` is given).
bool readMapObject(ClassicReader& r, protobuf::protocol::AppearanceInstance* out, uint16_t id = 0);
/// A field's objects up to its end marker; its low byte is the field's skip count.
bool readMapField(ClassicReader& r, protobuf::protocol::MapField* out);

/// Fill a world map message (FullMap, the four slices, TopFloor, BottomFloor,
/// FieldData, CreateOnMap, ChangeOnMap, DeleteOnMap); 0 when the body is short.
size_t fillMapMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
/// Commit the central position the last translated world map message moved to.
void advanceMapViewport(const protobuf::protocol::GameserverMessage& msg);
bool isMapMessage(int opcode);
/// Fill a creature message (InspectionState, CreatureUpdate, CreatureHealth,
/// CreatureLight, CreatureOutfit, CreatureSpeed, CreatureSkull, CreatureParty,
/// CreatureUnpass, CreatureMarks, CreatureType, SnapBack, Wait); 0 when the
/// body is short. The single-value creature messages carry the known creature's
/// data with that value.
size_t fillCreatureMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isCreatureMessage(int opcode);
/// Centre the translator's view (a floor description re-centring the map).
void setMapViewport(int x, int y, int z);

}
