#pragma once

// CIP-named bossbestiary surfaces (promoted from cipwire).

#include "bossbestiary/bosstiarystorage.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace bossbestiary {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBossBestiaryProtocolMessageHandler final : public cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler::TBossBestiaryProtocolMessageHandler;
};

// TBossTrackerStorage — bosstrackerstorage.h
// TBosstiaryStorage — bosstiarystorage.h

}
}

