#include "protocol/protobufclientmessagetranslator.h"

#include "protocol/protocolwriter.h"
#include "shared/iodevicewriter.h"

namespace tibia::protocol {

TProtobufClientMessageTranslator::TProtobufClientMessageTranslator(std::shared_ptr<TProtocolWriter> ProtocolWriter)
    : m_protocolWriter(std::move(ProtocolWriter))
    , m_messageWriter(m_protocolWriter ? m_protocolWriter->messageWriter() : nullptr)
{
}

TProtobufClientMessageTranslator::~TProtobufClientMessageTranslator() = default;

void TProtobufClientMessageTranslator::writeClientMessage(
    const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage)
{
    m_messageWriter->writeRawBytes(translateToClassic(GameclientMessage));
}

}
