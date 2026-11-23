#pragma once

#include "shared/experiencegainchangetracker.h"

namespace tibia::shared {

/// CIP shared::TNullExperienceGainChangeTracker: never reports a change.
class TNullExperienceGainChangeTracker final : public IExperienceGainChangeTracker
{
public:
    bool update(double) override { return false; }
};

}
