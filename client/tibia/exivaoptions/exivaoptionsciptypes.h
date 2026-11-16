#pragma once

// CIP-named exivaoptions surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace exivaoptions {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TExivaOptionsGameActionHandler final : public cip::tibia::exivaoptions::TExivaOptionsGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::exivaoptions::TExivaOptionsGameActionHandler::TExivaOptionsGameActionHandler;
};

class TExivaOptionsProtocolMessageHandler final : public cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler::TExivaOptionsProtocolMessageHandler;

    /// A suppressed exiva only flashes the chat's exiva button.
    static void handleExivaSuppressedMessage(const ::tibia::protobuf::protocol::GameserverMessageExivaSuppressed& message);
    /// The flags replace the stored ones; the listed names join or leave the whitelists.
    static void handleUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& message);
};

}
}

