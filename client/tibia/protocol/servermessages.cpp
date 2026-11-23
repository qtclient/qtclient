// CIP protocol::TProtocolServerMessageProcessor: splits a decoded server
// packet into messages, translates each one and puts it on the protocol queue.

#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"

#include "network/gameserverconnection.h"

#include "clockservice.h"

#include <protocol.pb.h>

#include <QtGlobal>
#include <format>
#include <idleot/stats.h>
#include <tibia/game/gamesession.h>

#include <algorithm>
#include <format>
#include <string>

namespace tibia::protocol {

namespace {

/// The packet the messages being read come from.
uint32_t g_packetNumber = 0;
int g_connectionType = 0;

// Product messages, outside the CIP protocol: the extended-opcode transport and
// the same-socket character switch. They return false when the message does
// not read.
bool readExtendedOpcode(ClassicReader& packet)
{
    uint8_t opcode = 0;
    std::string buffer;
    if (!packet.u8(opcode) || !packet.string(buffer))
        return false;
    if (opcode == 0) {
        if (auto* processor = network::TGameserverSessionNetwork::live())
            processor->setProductTransportEnabled(true);
        return true;
    }
    if (opcode == 2) {
        tibia::game::processPingBack();
        return true;
    }
    ExtendedOpcodeReader::instance().publish(opcode, buffer);
    return true;
}

bool readCharacterSwitch(ClassicReader& packet)
{
    std::string characterName;
    if (!packet.string(characterName))
        return false;
    tibia::game::processCharacterSwitch(characterName);
    // The server follows with a full login burst for the new body on this socket.
    if (auto* processor = network::TGameserverSessionNetwork::live())
        processor->setGameInitialized(false);
    return true;
}

}

void setServerPacketOrigin(const uint32_t PacketNumber, const int ConnectionType)
{
    g_packetNumber = PacketNumber;
    g_connectionType = ConnectionType;
}

void setServerMessageMetaData(::tibia::protobuf::protocol::GameserverMessage& message)
{
    auto* metaData = message.mutable_meta_data();
    metaData->set_timestamp(shared::TClockService::frameMillis());
    metaData->set_packet_number(g_packetNumber);
    metaData->set_value3(g_connectionType);
}

void processServerPacket(const uint8_t* data, const size_t size)
{
    auto* translator = serverMessageTranslator();
    if (!translator)
        return;
    ClassicReader packet{ data, size, 0 };
    while (packet.remaining() > 0) {
        uint8_t opcode = 0;
        packet.u8(opcode);
        AutoStat stat(STATS_PACKETS, std::format("{} (0x{:02X})", opcode, opcode));

        bool read = false;
        if (opcode == Proto::GameServerExtendedOpcode)
            read = readExtendedOpcode(packet);
        else if (opcode == Proto::GameServerCharacterSwitch)
            read = readCharacterSwitch(packet);
        else if (translator->hasStructuredLayout(opcode))
            read = translateAndEnqueueServerMessage(opcode, packet);

        // The message length is unknown, so the rest of the packet cannot be read.
        if (!read)
            return;
    }
}

}
