#pragma once

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageShowGameNews;
}

namespace tibia {
namespace news {

/// CIP news::TNewsProtocolMessageHandler: the server asks to show news of a
/// type (or the news dialog when the client has no such entry).
class TNewsStorage;

class TNewsProtocolMessageHandler final : public cip::tibia::news::TNewsProtocolMessageHandler
{
    Q_OBJECT
public:
    /// Made without a parent and without a news storage.
    TNewsProtocolMessageHandler();

    /// The news storage whose entries decide which news type is shown.
    void setNewsStorage(std::shared_ptr<TNewsStorage> NewsStorage);

    void handleShowGameNewsMessage(const ::tibia::protobuf::protocol::GameserverMessageShowGameNews& ShowGameNewsMessage);

private:
    std::shared_ptr<TNewsStorage> m_pNewsStorage;
};

}
}
