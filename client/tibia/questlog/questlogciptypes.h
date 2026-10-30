#pragma once

// CIP-named questlog surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::questlog {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TQuestLogGameActionHandler final : public cip::tibia::questlog::TQuestLogGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::questlog::TQuestLogGameActionHandler::TQuestLogGameActionHandler;
};

class TQuestLogProtocolMessageHandler final : public cip::tibia::questlog::TQuestLogProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::questlog::TQuestLogProtocolMessageHandler::TQuestLogProtocolMessageHandler;
};

class TQuestLogStorage final : public cip::tibia::questlog::TQuestLogStorage
{
    Q_OBJECT
public:
    using cip::tibia::questlog::TQuestLogStorage::TQuestLogStorage;
};

} // namespace tibia::questlog

