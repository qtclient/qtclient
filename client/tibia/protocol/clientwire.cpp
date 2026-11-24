// Classic wire -> protobuf for the client messages a session dump recorded
// (TProtobufServerMessageTranslator::readGameclientMessage and its readers).

#include "protocol/protobufobjectpositionhelper.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolreader.h"
#include "shared/iodevicereader.h"

#include "assets/things.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;
using shared::TIODeviceReader;

/// The object position of a classic coordinate.
void readObjectPosition(TProtocolReader& ProtocolReader, ObjectPosition* Target)
{
    const worldmap::TWorldMapCoordinate coordinate = ProtocolReader.readCoordinate();
    TProtobufObjectPositionHelper().fromClassicCoordinate(Target, coordinate.toPosition());
}

/// An object of a client message: its position, its type and the byte after
/// the type (stack position, liquid or tier).
void readObject(TProtocolReader& ProtocolReader, TIODeviceReader& Reader, ObjectPosition* Target,
                ObjectIdentifier* Identifier)
{
    readObjectPosition(ProtocolReader, Target);
    Identifier->set_type_id(Reader.readUInt16());
    Identifier->set_data(Reader.readUInt8());
}

/// Object id, then its tier when the object is tiered.
template<typename Entry>
void readTieredObject(TIODeviceReader& Reader, Entry* Object)
{
    const uint16_t objectId = Reader.readUInt16();
    Object->set_object_id(objectId);
    if (appearances::TAppearancesManager::instance().specialAppearancesForProtocolParsing().isUpgradeable(objectId))
        Object->set_tier(Reader.readUInt8());
}

void readSetClientOptions(TIODeviceReader& Reader, GameclientMessageSetClientOptions& Message)
{
    // Option 0 is the only one: turn towards the move direction.
    if (Reader.readUInt8() == 0)
        Message.set_turn_towards_move_direction(Reader.readBool());
}

void readTrackImbuements(TIODeviceReader& Reader, GameclientMessageTrackImbuements& Message)
{
    Message.set_open(Reader.readBool());
}

void readGoPath(TIODeviceReader& Reader, GameclientMessageGoPath& Message)
{
    for (uint8_t count = Reader.readUInt8(); count != 0; --count)
        Message.add_directions(Reader.readUInt8());
}

void readCharacterTradeConfigurationAction(TIODeviceReader& Reader,
                                           GameclientMessageCharacterTradeConfigurationAction& Message)
{
    const uint8_t action = Reader.readUInt8();
    Message.set_action(action);
    if ((action & 0xfe) != 2)
        return;
    // Set up or change an offer: price, end, the offered objects, store objects
    // and highlighted objects.
    Message.set_price(Reader.readUInt32());
    Message.set_end_time(Reader.readUInt32());
    for (uint8_t count = Reader.readUInt8(); count != 0; --count)
        readTieredObject(Reader, Message.add_objects());
    for (uint8_t count = Reader.readUInt8(); count != 0; --count)
        readTieredObject(Reader, Message.add_store_objects());
    for (uint8_t count = Reader.readUInt8(); count != 0; --count)
        Message.add_highlights(Reader.readUInt16());
}

void readMoveObject(TProtocolReader& ProtocolReader, TIODeviceReader& Reader, GameclientMessageMoveObject& Message)
{
    readObject(ProtocolReader, Reader, Message.mutable_source_object_position(), Message.mutable_object_identifier());
    readObjectPosition(ProtocolReader, Message.mutable_destination_object_position());
    Message.set_count(Reader.readUInt8());
    Message.set_extra_flag(Reader.readBool());
}

void readUseObject(TProtocolReader& ProtocolReader, TIODeviceReader& Reader, GameclientMessageUseObject& Message)
{
    readObject(ProtocolReader, Reader, Message.mutable_object_position(), Message.mutable_object_identifier());
    Message.set_container_index(Reader.readUInt8());
}

void readCloseContainer(TIODeviceReader& Reader, GameclientMessageCloseContainer& Message)
{
    Message.set_container_id(Reader.readUInt8());
}

void readUpContainer(TIODeviceReader& Reader, GameclientMessageUpContainer& Message)
{
    Message.set_container_id(Reader.readUInt8());
}

void readLook(TProtocolReader& ProtocolReader, TIODeviceReader& Reader, GameclientMessageLook& Message)
{
    readObject(ProtocolReader, Reader, Message.mutable_object_position(), Message.mutable_object_identifier());
}

void readQuickLoot(TProtocolReader& ProtocolReader, TIODeviceReader& Reader, GameclientMessageQuickLoot& Message)
{
    const uint8_t variant = Reader.readUInt8();
    Message.set_variant(variant);
    // Variants 0 and 1 (one corpse, all corpses around) name the clicked object.
    if (variant < 2) {
        ObjectIdentifierAndPosition* object = Message.mutable_object();
        readObject(ProtocolReader, Reader, object->mutable_object_position(), object->mutable_object_identifier());
    }
}

void readQuickLootBlackWhitelist(TIODeviceReader& Reader, GameclientMessageQuickLootBlackWhitelist& Message)
{
    Message.set_filter(Reader.readUInt8());
    for (uint16_t count = Reader.readUInt16(); count != 0; --count)
        Message.add_object_ids(Reader.readUInt16());
}

void readTalk(TIODeviceReader& Reader, GameclientMessageTalk& Message)
{
    const uint8_t mode = Reader.readUInt8();
    Message.set_mode(mode);
    switch (mode) {
    case 5:
    case 0x10:
        // Private messages name their receiver.
        Message.set_receiver(Reader.readString16().toStdString());
        break;
    case 7:
    case 0xe:
        // Channel messages name their channel.
        Message.set_channel_id(Reader.readUInt16());
        break;
    default:
        break;
    }
    Message.set_text(Reader.readString16().toStdString());
}

void readSetTactics(TIODeviceReader& Reader, GameclientMessageSetTactics& Message)
{
    Message.set_chase_opponent(Reader.readBool());
    Message.set_secure_mode(Reader.readBool());
    Message.set_pvp_mode(Reader.readUInt8());
}

/// Host: CipSoft keeps the town of action 0 and the new owner of action 3 in
/// two string fields; qtclient's message has one name for both.
void readCyclopediaHouseAction(TIODeviceReader& Reader, GameclientMessageCyclopediaHouseAction& Message)
{
    const uint8_t action = Reader.readUInt8();
    Message.set_action(action);
    if (action == 0) {
        Message.set_name(Reader.readString16().toStdString());
        return;
    }
    Message.set_house_id(Reader.readUInt32());
    switch (action) {
    case 2:
        Message.set_timestamp(Reader.readUInt32());
        break;
    case 3:
        Message.set_timestamp(Reader.readUInt32());
        Message.set_name(Reader.readString16().toStdString());
        Message.set_bid(Reader.readUInt64());
        break;
    case 1:
        Message.set_bid(Reader.readUInt64());
        break;
    default:
        break;
    }
}

void readClientDetails(TIODeviceReader& Reader, GameclientMessageClientDetails& Message)
{
    Message.set_details(Reader.readString16().toStdString());
}

void readInspectPlayer(TIODeviceReader& Reader, GameclientMessageInspectPlayer& Message)
{
    const uint8_t tab = Reader.readUInt8();
    Message.set_tab(tab);
    // Tabs 1..5 name the inspected player.
    if (static_cast<uint8_t>(tab - 1) < 5)
        Message.set_creature_id(Reader.readUInt32());
}

void readTrackQuestFlags(TIODeviceReader& Reader, GameclientMessageTrackQuestFlags& Message)
{
    for (uint8_t count = Reader.readUInt8(); count != 0; --count)
        Message.add_mission_ids(Reader.readUInt16());
    Message.set_auto_track_new_quests(Reader.readBool());
    Message.set_auto_untrack_completed_quests(Reader.readBool());
}

void readRequestResourceBalance(TIODeviceReader& Reader, GameclientMessageRequestResourceBalance& Message)
{
    Message.set_resource_type(Reader.readUInt8());
}

void readGetObjectInfo(TIODeviceReader& Reader, GameclientMessageGetObjectInfo& Message)
{
    for (uint8_t count = Reader.readUInt8(); count != 0; --count) {
        ProtocolObjectInfoRequest* object = Message.add_objects();
        object->set_object_id(Reader.readUInt16());
        object->set_data(Reader.readUInt8());
    }
}

void readMarketBrowse(TIODeviceReader& Reader, GameclientMessageMarketBrowse& Message)
{
    const uint8_t browseType = Reader.readUInt8();
    Message.set_browse_type(browseType);
    // Browse type 3 is one object's offers.
    if (browseType == 3)
        readTieredObject(Reader, &Message);
}

void readMarketCreate(TIODeviceReader& Reader, GameclientMessageMarketCreate& Message)
{
    Message.set_offer_type(Reader.readUInt8());
    readTieredObject(Reader, &Message);
    Message.set_amount(Reader.readUInt16());
    Message.set_price(Reader.readUInt64());
    Message.set_anonymous(Reader.readBool());
}

/// An offer: the time it was created and the counter of that second.
template<typename Message>
void readOfferId(TIODeviceReader& Reader, Message& Offer)
{
    Offer.set_timestamp(Reader.readUInt32());
    Offer.set_counter(Reader.readUInt16());
}

void readMarketCancel(TIODeviceReader& Reader, GameclientMessageMarketCancel& Message)
{
    readOfferId(Reader, Message);
}

void readMarketAccept(TIODeviceReader& Reader, GameclientMessageMarketAccept& Message)
{
    readOfferId(Reader, Message);
    Message.set_amount(Reader.readUInt16());
}

}

std::shared_ptr<GameclientMessage> TProtobufServerMessageTranslator::readGameclientMessage()
{
    TIODeviceReader& reader = *m_messageReader;
    TProtocolReader& protocolReader = *m_protocolReader;
    const uint8_t opcode = reader.readUInt8();
    auto message = std::make_shared<GameclientMessage>();
    message->set_type(opcode);
    GameclientMessage& m = *message;
    switch (opcode) {
    case 0x0f:
        m.MutableExtension(GameclientMessageEnterWorld::gameclient_message_enter_world);
        break;
    case 0x14:
        m.MutableExtension(GameclientMessageQuitGame::gameclient_message_quit_game);
        break;
    case 0x1d:
        m.MutableExtension(GameclientMessagePing::gameclient_message_ping);
        break;
    case 0x1e:
        m.MutableExtension(GameclientMessagePingBack::gameclient_message_ping_back);
        break;
    case 0x2e:
        readSetClientOptions(
            reader, *m.MutableExtension(GameclientMessageSetClientOptions::gameclient_message_set_client_options));
        break;
    case 0x60:
        readTrackImbuements(
            reader, *m.MutableExtension(GameclientMessageTrackImbuements::gameclient_message_track_imbuements));
        break;
    case 0x64:
        readGoPath(reader, *m.MutableExtension(GameclientMessageGoPath::gameclient_message_go_path));
        break;
    case 0x65:
        m.MutableExtension(GameclientMessageGoNorth::gameclient_message_go_north);
        break;
    case 0x66:
        m.MutableExtension(GameclientMessageGoEast::gameclient_message_go_east);
        break;
    case 0x67:
        m.MutableExtension(GameclientMessageGoSouth::gameclient_message_go_south);
        break;
    case 0x68:
        m.MutableExtension(GameclientMessageGoWest::gameclient_message_go_west);
        break;
    case 0x69:
        m.MutableExtension(GameclientMessageStop::gameclient_message_stop);
        break;
    case 0x6a:
        m.MutableExtension(GameclientMessageGoNorthEast::gameclient_message_go_north_east);
        break;
    case 0x6b:
        m.MutableExtension(GameclientMessageGoSouthEast::gameclient_message_go_south_east);
        break;
    case 0x6c:
        m.MutableExtension(GameclientMessageGoSouthWest::gameclient_message_go_south_west);
        break;
    case 0x6d:
        m.MutableExtension(GameclientMessageGoNorthWest::gameclient_message_go_north_west);
        break;
    case 0x6f:
        m.MutableExtension(GameclientMessageRotateNorth::gameclient_message_rotate_north);
        break;
    case 0x70:
        m.MutableExtension(GameclientMessageRotateEast::gameclient_message_rotate_east);
        break;
    case 0x71:
        m.MutableExtension(GameclientMessageRotateSouth::gameclient_message_rotate_south);
        break;
    case 0x72:
        m.MutableExtension(GameclientMessageRotateWest::gameclient_message_rotate_west);
        break;
    case 0x76:
        readCharacterTradeConfigurationAction(
            reader, *m.MutableExtension(GameclientMessageCharacterTradeConfigurationAction::
                                            gameclient_message_character_trade_configuration_action));
        break;
    case 0x78:
        readMoveObject(protocolReader, reader,
                       *m.MutableExtension(GameclientMessageMoveObject::gameclient_message_move_object));
        break;
    case 0x82:
        readUseObject(protocolReader, reader,
                      *m.MutableExtension(GameclientMessageUseObject::gameclient_message_use_object));
        break;
    case 0x87:
        readCloseContainer(reader,
                           *m.MutableExtension(GameclientMessageCloseContainer::gameclient_message_close_container));
        break;
    case 0x88:
        readUpContainer(reader, *m.MutableExtension(GameclientMessageUpContainer::gameclient_message_up_container));
        break;
    case 0x8c:
        readLook(protocolReader, reader, *m.MutableExtension(GameclientMessageLook::gameclient_message_look));
        break;
    case 0x8f:
        readQuickLoot(protocolReader, reader,
                      *m.MutableExtension(GameclientMessageQuickLoot::gameclient_message_quick_loot));
        break;
    case 0x91:
        readQuickLootBlackWhitelist(
            reader, *m.MutableExtension(
                        GameclientMessageQuickLootBlackWhitelist::gameclient_message_quick_loot_black_whitelist));
        break;
    case 0x96:
        readTalk(reader, *m.MutableExtension(GameclientMessageTalk::gameclient_message_talk));
        break;
    case 0xa0:
        readSetTactics(reader, *m.MutableExtension(GameclientMessageSetTactics::gameclient_message_set_tactics));
        break;
    case 0xad:
        readCyclopediaHouseAction(
            reader,
            *m.MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action));
        break;
    case 0xc1:
        readClientDetails(reader,
                          *m.MutableExtension(GameclientMessageClientDetails::gameclient_message_client_details));
        break;
    case 0xce:
        readInspectPlayer(reader,
                          *m.MutableExtension(GameclientMessageInspectPlayer::gameclient_message_inspect_player));
        break;
    case 0xd0:
        readTrackQuestFlags(
            reader, *m.MutableExtension(GameclientMessageTrackQuestFlags::gameclient_message_track_quest_flags));
        break;
    case 0xed:
        readRequestResourceBalance(
            reader,
            *m.MutableExtension(GameclientMessageRequestResourceBalance::gameclient_message_request_resource_balance));
        break;
    case 0xf3:
        readGetObjectInfo(reader,
                          *m.MutableExtension(GameclientMessageGetObjectInfo::gameclient_message_get_object_info));
        break;
    case 0xf5:
        readMarketBrowse(reader, *m.MutableExtension(GameclientMessageMarketBrowse::gameclient_message_market_browse));
        break;
    case 0xf6:
        readMarketCreate(reader, *m.MutableExtension(GameclientMessageMarketCreate::gameclient_message_market_create));
        break;
    case 0xf7:
        readMarketCancel(reader, *m.MutableExtension(GameclientMessageMarketCancel::gameclient_message_market_cancel));
        break;
    case 0xf8:
        readMarketAccept(reader, *m.MutableExtension(GameclientMessageMarketAccept::gameclient_message_market_accept));
        break;
    default:
        m.MutableExtension(GameclientMessageUnknown::gameclient_message_unknown)->set_opcode(opcode);
        break;
    }
    return message;
}

}
