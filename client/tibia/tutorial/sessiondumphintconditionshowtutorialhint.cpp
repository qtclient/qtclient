#include "tutorial/sessiondumphintconditionshowtutorialhint.h"

#include "input/gameaction.h"

namespace tibia::tutorial {

TSessiondumpHintConditionShowTutorialHint::TSessiondumpHintConditionShowTutorialHint(const int tutorialHintId)
    : TSessiondumpHintCondition(std::make_shared<input::TGameActionTutorialHintConfirmed>(tutorialHintId))
    , m_tutorialHintId(tutorialHintId)
{
}

input::TGameActionList TSessiondumpHintConditionShowTutorialHint::execute()
{
    input::TGameActionList actions;
    if (m_state == NotStarted)
        actions.push_back(std::make_shared<input::TGameActionShowTutorialHint>(m_tutorialHintId));
    const input::TGameActionList conditionActions = TSessiondumpHintCondition::execute();
    actions.insert(actions.end(), conditionActions.begin(), conditionActions.end());
    return actions;
}

}
