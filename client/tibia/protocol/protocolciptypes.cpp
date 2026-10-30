#include "protocol/protocolciptypes.h"

namespace tibia::protocol {

namespace {
struct CipTypedKeep {
    TGameActionContextHelper tGameActionContextHelper;
    TProtobufClientMessageTranslator tProtobufClientMessageTranslator;
    TProtobufObjectPositionHelper tProtobufObjectPositionHelper;
    TProtobufServerMessageTranslator tProtobufServerMessageTranslator;
    TProtocolClientMessageProcessor tProtocolClientMessageProcessor;
    TProtocolReader tProtocolReader;
    TProtocolServerMessageProcessor tProtocolServerMessageProcessor;
    TProtocolWriter tProtocolWriter;
    TUnknownGameserverException tUnknownGameserverException;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    new TProtocolMessageQueue(parent);
}

} // namespace tibia::protocol
