#pragma once

// CIP-named teamfinder surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::teamfinder {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TTeamFinderProtocolMessageHandler final : public cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler::TTeamFinderProtocolMessageHandler;
};

class TTeamFinderStorage final : public cip::tibia::teamfinder::TTeamFinderStorage
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamFinderStorage::TTeamFinderStorage;
};

class TTeamStartTimeManager final : public cip::tibia::teamfinder::TTeamStartTimeManager
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamStartTimeManager::TTeamStartTimeManager;
};

} // namespace tibia::teamfinder

