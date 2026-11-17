#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageShowGameNews;
}

namespace tibia {
namespace news {

/// CIP news::TNewsProtocolMessageHandler: the server asks to show news of a
/// type (or the news dialog when the client has no such entry).
class TNewsProtocolMessageHandler final : public cip::tibia::news::TNewsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::news::TNewsProtocolMessageHandler::TNewsProtocolMessageHandler;
    static void handleShowGameNewsMessage(const ::tibia::protobuf::protocol::GameserverMessageShowGameNews& ShowGameNewsMessage);
};

}
}
