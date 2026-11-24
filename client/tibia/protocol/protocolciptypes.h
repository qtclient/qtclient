#pragma once

// CIP-named protocol surfaces (Phase 6.0 load-split).
// Architecture: TR → TProtocolMessageQueue → handlers (classic bridge + native).

#include "protocol/protocolmessagequeue.h"
#include "worldmap/coordinates.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessage;
class GameclientMessage;
class ObjectIdentifier;
class ObjectPosition;
} // namespace protocol
} // namespace protobuf
namespace shared {
class TIODeviceReader;
} // namespace shared
} // namespace tibia

namespace tibia {
namespace protocol {

struct ClassicReader;
class TProtocolReader;

/// CIP protocol::TProtobufServerMessageTranslator: reads the messages of a
/// stream through its protocol reader: classic server messages (the game
/// server's wire, a classic session dump), protobuf server messages (a
/// protobuf session dump), and the classic and protobuf client messages a
/// session dump recorded.
///
/// Host: the classic server message fills read a byte span (ClassicReader);
/// readGameserverMessage peeks the stream's unread bytes into one. A known
/// message that does not read gives null there (CipSoft's reader throws). The tier byte of objects is decided by the appearances manager's
/// protocol parsing set, which CipSoft's ctor copies in.
class TProtobufServerMessageTranslator {
public:
    explicit TProtobufServerMessageTranslator(std::shared_ptr<TProtocolReader> ProtocolReader);
    virtual ~TProtobufServerMessageTranslator() = default;

    /// Reads the opcode and the message's fields. A message of an opcode it
    /// does not know has only the Unknown extension, with that opcode.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> translateServerMessage(
        ClassicReader& Reader);
    /// The central position of the map the map messages read so far moved to
    /// (its coordinate, the aware area's size and the centre's offset in it).
    virtual worldmap::TWorldMapCentralPosition centralPosition() const;
    /// Host: translateServerMessage over the stream's unread bytes; the stream
    /// moves past what was read. A product message outside the CIP protocol
    /// (extended opcode, character switch) is read and handled here and gives
    /// null. When a message does not read, the rest of the stream cannot be
    /// read (its length is unknown): it is dropped and null is returned.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> readGameserverMessage();
    /// A protobuf server message of `Size` bytes; throws std::invalid_argument
    /// when the bytes are no GameserverMessage.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> readGameserverProtobufMessage(
        uint16_t Size);
    /// A classic client message: the opcode, then its fields. A message of an
    /// opcode it does not know has only the Unknown extension, with that opcode.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameclientMessage> readGameclientMessage();
    /// A protobuf client message of `Size` bytes; throws std::invalid_argument
    /// when the bytes are no GameclientMessage.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameclientMessage> readGameclientProtobufMessage(
        uint16_t Size);

    QByteArray translateToClassic(
        const ::tibia::protobuf::protocol::GameserverMessage& message) const;

    static bool hasStructuredLayout(int opcode);

private:
    std::shared_ptr<TProtocolReader> m_protocolReader;
    std::shared_ptr<shared::TIODeviceReader> m_messageReader;
    worldmap::TWorldMapCentralPosition m_centralPosition;
};

/// The game client's current session's protocol message queue; null without a session.
TProtocolMessageQueue* protocolMessageQueue();


/// Empty structured server bodies (measure consumes 0 payload bytes on success).
bool emptyStructuredServerOpcode(int opcode);

/// Host: a decoded server packet that has no network stack behind it (a replay
/// of a classic packet log, or a session that hands its packets to the
/// classic session state): its messages are read like a live packet's and put
/// on the current session's queue.
void processServerPacket(const uint8_t* data, size_t size);
/// Host: the product messages outside the CIP protocol (the extended-opcode
/// transport and the same-socket character switch).
enum class EProductMessage
{
    /// The next message is a CIP message.
    None,
    Read,
    /// A product message that does not read: the rest of the packet cannot be read.
    Unreadable,
};
EProductMessage readProductMessage(ClassicReader& Reader);
/// dispatch.gen.cpp: every CIP server message has a handler / every client message a builder.
bool isHandledServerMessage(uint8_t opcode);
bool bindClientMessage(uint8_t opcode);



/// CIP outbound: a game-built client message, encoded by the client
/// translator and queued for the wire.
void sendGameclientMessage(std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> message);
/// A client message of `opcode` with no fields set yet.
std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> newGameclientMessage(int opcode);

}
}
