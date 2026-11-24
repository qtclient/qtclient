// Test double for TSessiondumpAsyncLoader::createProtocolMessageConsumer: the
// client's version (sessiondump/sessiondumpasyncloaderprotocol.cpp) builds
// CipSoft's protocol reader -> translator -> server message processor ->
// message queue chain, which needs the whole client. The dumps this test plays
// are protobuf (recording mode 1), so the double parses each server message's
// bytes and hands it to the loader's signal.
#include "sessiondump/sessiondumpasyncloader.h"
#include "sessiondump/sessiondumpmessageprocessor.h"
#include "shared/iodevicereader.h"

#include <protocol.pb.h>

#include <stdexcept>

namespace tibia::sessiondump {

namespace {

class TProtobufServerMessageConsumer final : public TSessiondumpMessageConsumer
{
public:
    TProtobufServerMessageConsumer(std::shared_ptr<shared::TIODeviceReader> device,
                                   std::function<void(TGameserverMessagePointer)> serverMessage)
        : m_device(std::move(device))
        , m_serverMessage(std::move(serverMessage))
    {
    }

    void processServerMessage(const uint64_t&, uint32_t, network::EConnectionType) override {}
    void processClientMessage(const uint64_t&) override {}
    void processServerMessage(const TSessiondumpMessageInfo& MessageInfo, uint32_t, network::EConnectionType) override
    {
        const QByteArray bytes = m_device->readBytes(MessageInfo.length);
        auto message = std::make_shared<protobuf::protocol::GameserverMessage>();
        if (!message->ParseFromArray(bytes.constData(), int(bytes.size())))
            throw std::runtime_error("Sessiondump message is no GameserverMessage");
        m_serverMessage(std::move(message));
    }

private:
    std::shared_ptr<shared::TIODeviceReader> m_device;
    std::function<void(TGameserverMessagePointer)> m_serverMessage;
};

}

void TSessiondumpAsyncLoader::createProtocolMessageConsumer()
{
    m_messageConsumer = std::make_shared<TProtobufServerMessageConsumer>(
        m_processingDevice, [this](TGameserverMessagePointer message) { Q_EMIT gameserverMessageAvailable(std::move(message)); });
}

}
