// Host entries of the server message path: the product messages outside the
// CIP protocol, and decoded server packets without a network stack behind them.

#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "protocol/protocolcodes.h"
#include "protocol/protocolreader.h"
#include "protocol/protocolserverpacketprocessor.h"
#include "protocol/protocolservermessageprocessor.h"
#include "tibia/protocol/extendedopcode.h"

#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "network/gameserverconnection.h"
#include "network/gameservernetworkpacket.h"
#include "network/unencryptedrawmessagestream.h"
#include "shared/iodevicereader.h"

#include <tibia/game/gamesession.h>

#include <string>

namespace tibia::protocol {

namespace {

// Both return false when the message does not read.
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

EProductMessage readProductMessage(ClassicReader& Reader)
{
    if (Reader.remaining() == 0)
        return EProductMessage::None;
    const uint8_t opcode = Reader.p[Reader.i];
    if (opcode != Proto::GameServerExtendedOpcode && opcode != Proto::GameServerCharacterSwitch)
        return EProductMessage::None;
    ++Reader.i;
    const bool read = opcode == Proto::GameServerExtendedOpcode ? readExtendedOpcode(Reader) : readCharacterSwitch(Reader);
    return read ? EProductMessage::Read : EProductMessage::Unreadable;
}

void processServerPacket(const uint8_t* data, const size_t size)
{
    const game::TGameSessionBase* session = game::currentGameSession();
    if (!session || !session->protocolMessageQueue())
        return;
    // A recorded packet log may hold anything: an unknown message ends its
    // packet instead of the session.
    auto messageStream = std::make_shared<network::TUnencryptedRawMessageStream>();
    auto protocolReader = std::make_shared<TProtocolReader>(
        std::make_shared<shared::TIODeviceReader>(messageStream, QDataStream::LittleEndian));
    auto serverMessageProcessor = std::make_shared<TProtocolServerMessageProcessor>(
        std::make_shared<TProtobufServerMessageTranslator>(std::move(protocolReader)), session->protocolMessageQueue(),
        false);
    TProtocolServerPacketProcessor packetProcessor(std::move(messageStream), std::move(serverMessageProcessor));
    packetProcessor.processPacket(network::TGameserverNetworkPacket(
        network::TGameserverNetworkPacket::Gameserver,
        QByteArray(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size))));
}

}
