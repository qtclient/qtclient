#pragma once

// CIP-named highscores surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::highscores {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class THighscoresProtocolMessageHandler final : public cip::tibia::highscores::THighscoresProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::highscores::THighscoresProtocolMessageHandler::THighscoresProtocolMessageHandler;
};

class THighscoresStorage final : public cip::tibia::highscores::THighscoresStorage
{
    Q_OBJECT
public:
    using cip::tibia::highscores::THighscoresStorage::THighscoresStorage;
};

} // namespace tibia::highscores

