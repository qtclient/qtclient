#pragma once

#include "input/gameaction.h"
#include "skillwheel/skillwheeldata.h"

namespace tibia {
namespace skillwheel {

/// CIP input::gameactions::TGameActionSaveSkillWheel (SAVE_SKILL_WHEEL): the configuration to save.
/// The generic makeAction carrier has no wheel; the page publishes this one.
class TGameActionSaveSkillWheelConfiguration final : public input::TGameAction
{
public:
    explicit TGameActionSaveSkillWheelConfiguration(const TWheel& configuration) : wheel(configuration)
    {
        type = input::EGameAction::SAVE_SKILL_WHEEL;
    }

    TWheel wheel;
};

}
}
