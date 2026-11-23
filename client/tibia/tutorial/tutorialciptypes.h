#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageTutorialHint;
}
}
}

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TTutorialProtocolMessageHandler`: turns the server's
/// TutorialHint into a tutorial hint dialog (SHOW_TUTORIAL_HINT).
class TTutorialProtocolMessageHandler final : public cip::tibia::tutorial::TTutorialProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TTutorialProtocolMessageHandler(QObject* parent = nullptr);

    /// Shows hint 1..14 (others are ignored). Before hints 7 to 10 and 12,
    /// the default minimap tiles are loaded from the resources.
    void handleTutorialHintMessage(
        const ::tibia::protobuf::protocol::GameserverMessageTutorialHint& TutorialHintMessage);
};

}
}
