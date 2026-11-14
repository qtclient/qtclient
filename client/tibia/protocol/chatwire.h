#pragma once

// Classic wire → protobuf for chat: talk, text messages and channels.

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia::protocol {

/// Fill a chat message (Talk, Channels, OpenChannel, PrivateChannel,
/// EditGuildMessage, OpenOwnChannel, CloseChannel, Message, ChannelEvent);
/// 0 when the body is short.
size_t fillChatMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isChatMessage(int opcode);

}
