#include "protocol/protobufclientmessagetranslator.h"

#include "protocol/protocolwriter.h"
#include "shared/iodevicewriter.h"

namespace tibia::protocol {

TProtobufClientMessageTranslator::TProtobufClientMessageTranslator(
    std::shared_ptr<TProtocolWriter> ProtocolWriter, std::shared_ptr<shared::TRsaHelper> RsaHelper,
    std::shared_ptr<TProtobufObjectPositionHelper> ObjectPositionHelper,
    const appearances::TSpecialAppearancesForProtocolParsing& SpecialAppearances)
    : m_protocolWriter(std::move(ProtocolWriter))
    , m_messageWriter(m_protocolWriter ? m_protocolWriter->messageWriter() : nullptr)
    , m_rsaHelper(std::move(RsaHelper))
    , m_objectPositionHelper(std::move(ObjectPositionHelper))
    , m_specialAppearances(SpecialAppearances)
{
}

TProtobufClientMessageTranslator::~TProtobufClientMessageTranslator() = default;

void TProtobufClientMessageTranslator::writeClientMessage(
    const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage)
{
    m_messageWriter->writeRawBytes(
        translateToClassic(GameclientMessage, m_rsaHelper.get(), m_objectPositionHelper.get()));
}

}
