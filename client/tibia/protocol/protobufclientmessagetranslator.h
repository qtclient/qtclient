#pragma once

#include <QByteArray>

#include <cstdint>
#include <memory>

namespace tibia::protobuf::protocol {
class GameclientMessage;
class ObjectIdentifier;
class ObjectPosition;
}

namespace tibia::shared {
class TIODeviceWriter;
}

namespace tibia {
namespace protocol {

class TProtocolWriter;

/// CIP protocol::TProtobufClientMessageTranslator: writes a client message in
/// the game server's classic wire format (its type byte, then its fields) to
/// its protocol writer's device.
///
/// Host: the fields are encoded by translateToClassic (the login's RSA block
/// with the login RSA helper, object positions with a
/// TProtobufObjectPositionHelper) and written as one block; CipSoft's
/// translator keeps both helpers and writes value by value.
class TProtobufClientMessageTranslator
{
public:
    explicit TProtobufClientMessageTranslator(std::shared_ptr<TProtocolWriter> ProtocolWriter);
    virtual ~TProtobufClientMessageTranslator();

    virtual void writeClientMessage(const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage);

    /// Host: the classic wire bytes of a client message; empty for a message
    /// without a type.
    static QByteArray translateToClassic(const ::tibia::protobuf::protocol::GameclientMessage& message);

    /// The classic stack position byte of an object: the stack position on a map
    /// field, the inventory slot, the plain container slot, the liquid or tier of
    /// an object "anywhere"; 0 otherwise.
    static uint8_t stackPosition(const ::tibia::protobuf::protocol::ObjectPosition& objectPosition,
                                 const ::tibia::protobuf::protocol::ObjectIdentifier& objectIdentifier);

private:
    std::shared_ptr<TProtocolWriter> m_protocolWriter;
    std::shared_ptr<shared::TIODeviceWriter> m_messageWriter;
};

}
}
