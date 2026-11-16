#include "tutorial/sessiondumphintpublishgameaction.h"

#include <memory>

namespace tibia::tutorial {

TSessiondumpHintPublishGameAction::TSessiondumpHintPublishGameAction(const EType type, input::TGameActionPtr action)
    : TSessiondumpHintBase(type)
    , m_action(std::move(action))
{
}

input::TGameActionList TSessiondumpHintPublishGameAction::execute()
{
    input::TGameActionList actions{ m_action };
    m_done = true;
    return actions;
}

namespace {

input::TGameActionPtr skipToAction(const int64_t timestamp)
{
    auto action = std::make_shared<input::TGameActionSessiondumpSkipTo>();
    action->parameter = int(timestamp);
    return action;
}

input::TGameActionPtr tutorialProgressAction(const QString& progress)
{
    auto action = std::make_shared<input::TGameActionTutorialProgress>();
    action->text = progress;
    return action;
}

}

TSessiondumpHintSkipTo::TSessiondumpHintSkipTo(const int64_t& skipToTimestamp)
    : TSessiondumpHintPublishGameAction(SkipTo, skipToAction(skipToTimestamp))
{
}

TSessiondumpHintSwitchChannel::TSessiondumpHintSwitchChannel(const int channelType)
    : TSessiondumpHintPublishGameAction(
          SwitchChannel, std::make_shared<input::TGameActionChatChannel>(input::EGameAction::OPEN_CHAT_CHANNEL, channelType, 0))
{
}

TSessiondumpHintTutorialProgress::TSessiondumpHintTutorialProgress(const QString& tutorialProgress)
    : TSessiondumpHintPublishGameAction(TutorialProgress, tutorialProgressAction(tutorialProgress))
{
}

TSessiondumpHintClearOldestMessage::TSessiondumpHintClearOldestMessage()
    : TSessiondumpHintPublishGameAction(ClearOldestMessage, std::make_shared<input::TGameActionClearOldestMessageFromMapwindow>())
{
}

}
