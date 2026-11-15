// CIP protocol::TProtocolServerMessageProcessor: splits a decoded server
// packet into messages, translates each one and puts it on the protocol queue.

#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"

#include "network/gameserverpacketprocessor.h"

#include <QtGlobal>
#include <format>
#include <idleot/stats.h>
#include <tibia/game/gamesession.h>

#include <algorithm>
#include <format>
#include <string>

namespace tibia::protocol {

namespace {

std::string hexPreview(const uint8_t* data, const size_t size)
{
    std::string out;
    for (size_t i = 0; i < std::min<size_t>(size, 32); ++i)
        out += std::format("{:02X} ", data[i]);
    return out;
}

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
        if (auto* processor = network::TGameserverNetworkPacketProcessor::live())
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
    if (auto* processor = network::TGameserverNetworkPacketProcessor::live())
        processor->setGameInitialized(false);
    return true;
}

}

void processServerPacket(const uint8_t* data, const size_t size)
{
    auto* translator = serverMessageTranslator();
    if (!translator)
        return;
    ClassicReader packet{ data, size, 0 };
    int previous = -1;
    while (packet.remaining() > 0) {
        const size_t begin = packet.i;
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

        if (!read) {
            // The message length is unknown, so the rest of the packet cannot be read.
            const size_t left = size - std::min(size, begin + 1);
            qCritical("%s", std::format("[{}] unreadable server message 0x{:02X} ({} bytes left, previous 0x{:02X}); next bytes: {}",
                           tibia::game::clientRevision(), opcode, left, previous, hexPreview(data + begin + 1, left)).c_str());
            return;
        }
        previous = opcode;
    }
}

}
