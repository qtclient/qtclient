#pragma once

#include "tutorial/sessiondumphintbase.h"

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TSessiondumpHintPublishGameAction`: publishes one game action.
class TSessiondumpHintPublishGameAction : public TSessiondumpHintBase
{
public:
    TSessiondumpHintPublishGameAction(EType type, input::TGameActionPtr action);

    /// The action; the hint is done afterwards.
    input::TGameActionList execute() override;

protected:
    input::TGameActionPtr m_action;
};

/// CIP `tutorial::TSessiondumpHintSkipTo`: jumps the replay to a timestamp.
class TSessiondumpHintSkipTo final : public TSessiondumpHintPublishGameAction
{
public:
    explicit TSessiondumpHintSkipTo(const int64_t& skipToTimestamp);
};

/// CIP `tutorial::TSessiondumpHintSwitchChannel`: opens the local chat, or the
/// server log for its channel id.
class TSessiondumpHintSwitchChannel final : public TSessiondumpHintPublishGameAction
{
public:
    explicit TSessiondumpHintSwitchChannel(int channelType);
};

/// CIP `tutorial::TSessiondumpHintTutorialProgress`.
class TSessiondumpHintTutorialProgress final : public TSessiondumpHintPublishGameAction
{
public:
    explicit TSessiondumpHintTutorialProgress(const QString& tutorialProgress);
};

/// CIP `tutorial::TSessiondumpHintClearOldestMessage`: clears the oldest map window message.
class TSessiondumpHintClearOldestMessage final : public TSessiondumpHintPublishGameAction
{
public:
    TSessiondumpHintClearOldestMessage();
};

}
}
