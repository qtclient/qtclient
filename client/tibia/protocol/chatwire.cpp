#include "protocol/chatwire.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

bool readCoordinate(ClassicReader& r, ProtocolCoordinate* out)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (!r.u16(x) || !r.u16(y) || !r.u8(z))
        return false;
    out->set_x(x);
    out->set_y(y);
    out->set_z(z);
    return true;
}

bool readNames(ClassicReader& r, google::protobuf::RepeatedPtrField<std::string>* out)
{
    uint16_t count = 0;
    if (!r.u16(count))
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        if (!r.string(*out->Add()))
            return false;
    }
    return true;
}

template<typename Message>
bool readChannel(ClassicReader& r, Message* m)
{
    uint16_t id = 0;
    if (!r.u16(id) || !r.string(*m->mutable_name()))
        return false;
    m->set_channel_id(id);
    return readNames(r, m->mutable_joined_players()) && readNames(r, m->mutable_invited_players());
}

/// Talk modes followed by the speaker's position (mask 0x3000300000060e).
bool talkHasPosition(const uint8_t mode)
{
    return mode < 0x36 && ((0x3000300000060eULL >> mode) & 1) != 0;
}

/// Talk modes followed by a channel id (mask 0x4180).
bool talkHasChannel(const uint8_t mode)
{
    return mode < 0x36 && ((0x4180ULL >> mode) & 1) != 0;
}

bool readTalk(ClassicReader& r, GameserverMessageTalk* m)
{
    uint32_t statement = 0;
    if (!r.u32(statement) || !r.string(*m->mutable_speaker()))
        return false;
    m->set_statement_id(statement);
    if (statement != 0) {
        uint8_t traded = 0;
        if (!r.u8(traded))
            return false;
        m->set_traded(traded != 0);
    }
    uint16_t level = 0;
    uint8_t mode = 0;
    if (!r.u16(level) || !r.u8(mode))
        return false;
    m->set_speaker_level(level);
    m->set_mode(mode);
    if (talkHasPosition(mode)) {
        if (!readCoordinate(r, m->mutable_position()))
            return false;
    } else if (talkHasChannel(mode)) {
        uint16_t channel = 0;
        if (!r.u16(channel))
            return false;
        m->set_channel_id(channel);
    }
    return r.string(*m->mutable_text());
}

bool readValueAndColor(ClassicReader& r, GameserverMessageMessage* m, const bool wide)
{
    uint8_t color = 0;
    if (wide) {
        uint64_t value = 0;
        if (!r.u64(value) || !r.u8(color))
            return false;
        m->set_value(static_cast<int64_t>(value));
    } else {
        uint32_t value = 0;
        if (!r.u32(value) || !r.u8(color))
            return false;
        m->set_value(static_cast<int32_t>(value));
    }
    m->set_color(color);
    return true;
}

bool readMessage(ClassicReader& r, GameserverMessageMessage* m)
{
    uint8_t mode = 0;
    if (!r.u8(mode))
        return false;
    m->set_mode(mode);
    switch (mode) {
    case 6: // channel management
    case 0x21: // guild
    case 0x22: // party management
    case 0x23: { // party
        uint16_t channel = 0;
        if (!r.u16(channel))
            return false;
        m->set_channel_id(channel);
        break;
    }
    case 0x17: // damage dealt
    case 0x18: // damage received
    case 0x1b: { // damage others
        uint32_t second = 0;
        uint8_t secondColor = 0;
        if (!readCoordinate(r, m->mutable_position()) || !readValueAndColor(r, m, false) || !r.u32(second)
            || !r.u8(secondColor))
            return false;
        m->set_second_value(static_cast<int32_t>(second));
        m->set_second_color(secondColor);
        break;
    }
    case 0x19: // heal
    case 0x1c: // heal others
    case 0x2b: // mana
        if (!readCoordinate(r, m->mutable_position()) || !readValueAndColor(r, m, false))
            return false;
        break;
    case 0x1a: // experience
    case 0x1d: // experience others
        if (!readCoordinate(r, m->mutable_position()) || !readValueAndColor(r, m, true))
            return false;
        break;
    default:
        break;
    }
    return r.string(*m->mutable_text());
}

}

bool isChatMessage(const int opcode)
{
    switch (opcode) {
    case 170: // Talk
    case 171: // Channels
    case 172: // OpenChannel
    case 173: // PrivateChannel
    case 174: // EditGuildMessage
    case 178: // OpenOwnChannel
    case 179: // CloseChannel
    case 180: // Message
    case 243: // ChannelEvent
        return true;
    default:
        return false;
    }
}

size_t fillChatMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 170:
        if (!readTalk(r, msg.MutableExtension(GameserverMessageTalk::gameserver_message_talk)))
            return 0;
        return r.i;
    case 171: {
        auto* m = msg.MutableExtension(GameserverMessageChannels::gameserver_message_channels);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolChannel* channel = m->add_channels();
            uint16_t id = 0;
            if (!r.u16(id) || !r.string(*channel->mutable_name()))
                return 0;
            channel->set_id(id);
        }
        return r.i;
    }
    case 172:
        if (!readChannel(r, msg.MutableExtension(GameserverMessageOpenChannel::gameserver_message_open_channel)))
            return 0;
        return r.i;
    case 173:
        if (!r.string(*msg.MutableExtension(GameserverMessagePrivateChannel::gameserver_message_private_channel)
                           ->mutable_name()))
            return 0;
        return r.i;
    case 174:
        if (!r.string(*msg.MutableExtension(GameserverMessageEditGuildMessage::gameserver_message_edit_guild_message)
                           ->mutable_text()))
            return 0;
        return r.i;
    case 178:
        if (!readChannel(r, msg.MutableExtension(GameserverMessageOpenOwnChannel::gameserver_message_open_own_channel)))
            return 0;
        return r.i;
    case 179: {
        uint16_t id = 0;
        if (!r.u16(id))
            return 0;
        msg.MutableExtension(GameserverMessageCloseChannel::gameserver_message_close_channel)->set_channel_id(id);
        return r.i;
    }
    case 180:
        if (!readMessage(r, msg.MutableExtension(GameserverMessageMessage::gameserver_message_message)))
            return 0;
        return r.i;
    case 243: {
        auto* m = msg.MutableExtension(GameserverMessageChannelEvent::gameserver_message_channel_event);
        uint16_t id = 0;
        uint8_t type = 0;
        if (!r.u16(id) || !r.string(*m->mutable_name()) || !r.u8(type))
            return 0;
        m->set_channel_id(id);
        m->set_event_type(type);
        return r.i;
    }
    default:
        return 0;
    }
}

}
