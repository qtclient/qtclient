#pragma once

#include "input/gameaction.h"
#include "skillwheel/skillwheeldata.h"

namespace tibia {
namespace input {
namespace gameactions {

/// CIP input::gameactions::TGameActionSaveSkillWheel (SAVE_SKILL_WHEEL): the
/// wheel configuration to save.
class TGameActionSaveSkillWheel final : public TGameActionSimple
{
public:
    explicit TGameActionSaveSkillWheel(const skillwheel::TWheel& configuration)
        : TGameActionSimple(EGameAction::SAVE_SKILL_WHEEL)
        , m_wheel(configuration)
    {
    }
    const skillwheel::TWheel& wheel() const { return m_wheel; }

private:
    skillwheel::TWheel m_wheel;
};

}
}
}
