#include "sessiondump/sessiondumpasyncloader.h"

#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include "protocol/protocolreader.h"
#include "protocol/protocolservermessageprocessor.h"
#include "shared/iodevicereader.h"

#include <protocol.pb.h>

namespace tibia::sessiondump {

void TSessiondumpAsyncLoader::createProtocolMessageConsumer()
{
    auto translator = std::make_shared<protocol::TProtobufServerMessageTranslator>(
        std::make_shared<protocol::TProtocolReader>(m_processingDevice));
    std::shared_ptr<protocol::TProtocolMessageQueueConfiguration> queueConfiguration;
    m_protocolMessageQueue = std::make_shared<protocol::TProtocolMessageQueue>(queueConfiguration);
    m_connections.push_back(connect(m_protocolMessageQueue.get(), &protocol::TProtocolMessageQueue::receivedMessage, this,
                                    [this](TGameserverMessagePointer message) {
                                        Q_EMIT gameserverMessageAvailable(std::move(message));
                                    }));
    m_connections.push_back(connect(m_protocolMessageQueue.get(), &protocol::TProtocolMessageQueue::sendMessage, this,
                                    [this](TGameclientMessagePointer message) {
                                        Q_EMIT gameclientMessageAvailable(std::move(message));
                                    }));
    m_messageConsumer =
        std::make_shared<protocol::TProtocolServerMessageProcessor>(std::move(translator), m_protocolMessageQueue, false);
}

}
