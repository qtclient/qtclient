#include "tutorial/sessiondumphintpublishgameaction.h"

#include "chat/chatchannel.h"

#include <memory>

namespace tibia::tutorial {

TSessiondumpHintPublishGameAction::TSessiondumpHintPublishGameAction(const EType type, input::IGameActionPtr action)
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

input::IGameActionPtr skipToAction(const int64_t timestamp)
{
    return std::make_shared<input::TGameActionSessiondumpSkipTo>(timestamp);
}

input::IGameActionPtr tutorialProgressAction(const QString& progress)
{
    return std::make_shared<input::TGameActionTutorialProgress>(progress);
}

}

TSessiondumpHintSkipTo::TSessiondumpHintSkipTo(const int64_t& skipToTimestamp)
    : TSessiondumpHintPublishGameAction(SkipTo, skipToAction(skipToTimestamp))
{
}

TSessiondumpHintSwitchChannel::TSessiondumpHintSwitchChannel(const int channelType)
    : TSessiondumpHintPublishGameAction(
          SwitchChannel,
          std::make_shared<input::TGameActionChatChannel>(
              input::EGameAction::OPEN_CHAT_CHANNEL,
              std::make_shared<const chat::TChatChannelIdentifier>(
                  chat::TChatChannelIdentifier{ static_cast<chat::TChatChannelIdentifier::Type>(channelType), 0, {} })))
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
