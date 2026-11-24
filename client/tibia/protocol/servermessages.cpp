// Host entries of the server message path: the product messages outside the
// CIP protocol.

#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "tibia/protocol/extendedopcode.h"

#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "network/producttransport.h"

#include <client/const.h>
#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include "game/objectreference.h"

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
        // The server opened the product transport of the live session.
        if (const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession())
            if (network::TProductTransport* transport = session->productTransport())
                transport->setOpen(true);
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
    // The server follows with a full login burst for the new body on this socket.
    if (tibia::game::TGameSessionBase* session = tibia::game::currentGameSession())
        session->processCharacterSwitch(characterName);
    return true;
}

}

EProductMessage readProductMessage(ClassicReader& Reader)
{
    if (Reader.remaining() == 0)
        return EProductMessage::None;
    // The product's messages: extended opcode (0x32) and character switch (0x0D).
    constexpr uint8_t ExtendedOpcode = 0x32;
    constexpr uint8_t CharacterSwitch = 0x0D;
    const uint8_t opcode = Reader.p[Reader.i];
    if (opcode != ExtendedOpcode && opcode != CharacterSwitch)
        return EProductMessage::None;
    ++Reader.i;
    const bool read = opcode == ExtendedOpcode ? readExtendedOpcode(Reader) : readCharacterSwitch(Reader);
    return read ? EProductMessage::Read : EProductMessage::Unreadable;
}

}
