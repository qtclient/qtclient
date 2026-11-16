#pragma once

#include "sessiondump/sessiondumpmessageprocessor.h"

#include <QByteArray>

#include <functional>

namespace tibia {
namespace sessiondump {

/// CIP `sessiondump::TLightweightSessiondumpMessageProcessor`: hands every
/// message to a callback with its packet, its header and a reader of its bytes.
class TLightweightSessiondumpMessageProcessor : public TSessiondumpMessageProcessor
{
public:
    using TCallback = std::function<void(const TSessiondumpPacketInfo&, const TSessiondumpMessageInfo&,
                                         const std::function<QByteArray()>&)>;

    TLightweightSessiondumpMessageProcessor(std::shared_ptr<TSessiondumpMessageReader> messageReader, TCallback callback);
    ~TLightweightSessiondumpMessageProcessor() override;

    void process(const TSessiondumpPacketInfo& packet) override;

private:
    TCallback m_callback;
};

}
}
