#pragma once

// CIP-named exaltation surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageCloseExaltationDialog;
class GameserverMessageExaltationBaseData;
class GameserverMessageExaltationDialogRefresh;
class GameserverMessageExaltationHistory;
class GameserverMessageExaltationResult;
}

namespace tibia {
namespace exaltation {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TExaltationGameActionHandler final : public cip::tibia::exaltation::TExaltationGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::exaltation::TExaltationGameActionHandler::TExaltationGameActionHandler;
};

class TExaltationProtocolMessageHandler final : public cip::tibia::exaltation::TExaltationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::exaltation::TExaltationProtocolMessageHandler::TExaltationProtocolMessageHandler;
    static void handleCloseExaltationDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseExaltationDialog& CloseExaltationDialogMessage);
    static void handleExaltationDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationDialogRefresh& ExaltationDialogRefreshMessage);
    static void handleExaltationBaseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationBaseData& ExaltationBaseDataMessage);
    static void handleExaltationResultMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationResult& ExaltationResultMessage);
    static void handleExaltationHistoryMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationHistory& ExaltationHistoryMessage);
public:
};

}
}

