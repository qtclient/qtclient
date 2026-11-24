#pragma once

#include "assets/specialappearances.h"

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
class TRsaHelper;
}

namespace tibia {
namespace protocol {

class TProtobufObjectPositionHelper;
class TProtocolWriter;

/// CIP protocol::TProtobufClientMessageTranslator: writes a client message in
/// the game server's classic wire format (its type byte, then its fields) to
/// its protocol writer's device. The login messages' RSA blocks are encrypted
/// with its RSA helper, object positions are converted by its object position
/// helper; it keeps a copy of the appearances' special ids for the protocol.
///
/// Host: the fields are encoded by translateToClassic and written as one
/// block; CipSoft's translator writes value by value.
class TProtobufClientMessageTranslator
{
public:
    TProtobufClientMessageTranslator(std::shared_ptr<TProtocolWriter> ProtocolWriter,
                                     std::shared_ptr<shared::TRsaHelper> RsaHelper,
                                     std::shared_ptr<TProtobufObjectPositionHelper> ObjectPositionHelper,
                                     const appearances::TSpecialAppearancesForProtocolParsing& SpecialAppearances);
    virtual ~TProtobufClientMessageTranslator();

    virtual void writeClientMessage(const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage);

    /// Host: the classic wire bytes of a client message, the RSA blocks
    /// encrypted with `rsaHelper` and object positions converted by
    /// `objectPositionHelper`; empty for a message without a type.
    static QByteArray translateToClassic(const ::tibia::protobuf::protocol::GameclientMessage& message,
                                         shared::TRsaHelper* rsaHelper,
                                         const TProtobufObjectPositionHelper* objectPositionHelper);
    /// Host: translateToClassic for a message sent before the session's
    /// protocol queue exists (the login RSA helper, a plain position helper).
    static QByteArray translateToClassic(const ::tibia::protobuf::protocol::GameclientMessage& message);

    /// The classic stack position byte of an object: the stack position on a map
    /// field, the inventory slot, the plain container slot, the liquid or tier of
    /// an object "anywhere"; 0 otherwise.
    static uint8_t stackPosition(const ::tibia::protobuf::protocol::ObjectPosition& objectPosition,
                                 const ::tibia::protobuf::protocol::ObjectIdentifier& objectIdentifier);

private:
    std::shared_ptr<TProtocolWriter> m_protocolWriter;
    std::shared_ptr<shared::TIODeviceWriter> m_messageWriter;
    std::shared_ptr<shared::TRsaHelper> m_rsaHelper;
    std::shared_ptr<TProtobufObjectPositionHelper> m_objectPositionHelper;
    appearances::TSpecialAppearancesForProtocolParsing m_specialAppearances;
};

}
}
