#pragma once

#include "tutorial/sessiondumphintcondition.h"

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TSessiondumpHintConditionShowTutorialHint`: shows a tutorial
/// hint dialog and holds the replay until the player confirms that hint.
class TSessiondumpHintConditionShowTutorialHint final : public TSessiondumpHintCondition
{
public:
    explicit TSessiondumpHintConditionShowTutorialHint(int tutorialHintId);

    /// The first run shows the hint, then publishes what the condition publishes.
    input::TGameActionList execute() override;

private:
    int m_tutorialHintId;
};

}
}
