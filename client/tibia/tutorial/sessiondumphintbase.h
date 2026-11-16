#pragma once

#include "input/gameaction.h"

#include <cstdint>

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TSessiondumpHintBase`: something the tutorial replay does at
/// a timestamp of the dump.
class TSessiondumpHintBase
{
public:
    /// The kind of hint (the JSON "type").
    enum EType
    {
        Condition = 1,
        SkipTo = 2,
        SwitchChannel = 3,
        TutorialProgress = 4,
        ClearOldestMessage = 5,
    };

    explicit TSessiondumpHintBase(EType type);
    virtual ~TSessiondumpHintBase();

    virtual EType type() const;
    /// Milliseconds into the dump.
    virtual int64_t timestamp() const;
    virtual void setTimestamp(const int64_t& timestamp);
    virtual bool isDone() const;
    virtual void setDone();
    /// The game actions to publish now; marks the hint done when it is.
    virtual input::TGameActionList execute() = 0;

protected:
    int64_t m_timestamp = 0;
    EType m_type;
    bool m_done = false;
};

}
}
