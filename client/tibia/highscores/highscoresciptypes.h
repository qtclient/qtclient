#pragma once

// CIP-named highscores surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace highscores {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class THighscoresProtocolMessageHandler final : public cip::tibia::highscores::THighscoresProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::highscores::THighscoresProtocolMessageHandler::THighscoresProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

class THighscoresStorage final : public cip::tibia::highscores::THighscoresStorage
{
    Q_OBJECT
public:
    using cip::tibia::highscores::THighscoresStorage::THighscoresStorage;
};

}
}

