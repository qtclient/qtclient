#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <memory>

namespace tibia {
namespace tutorial {

class TSessiondumpHints;
class TSessiondumpHintBase;

/// CIP `tutorial::TSessiondumpHintsProcessor`: runs the hints of a tutorial
/// dump as the replay reaches them. It keeps the first unfinished hint. The
/// session processes the hints due at the replay's progress, and the
/// processor publishes their game actions. A hint that is not done yet (a
/// condition waiting for the player) stays the current one, and the replay
/// waits at it. The router's game actions reach the current condition. A
/// sessiondump skip drops the hints before its target.
class TSessiondumpHintsProcessor : public cip::tibia::tutorial::TSessiondumpHintsProcessor
{
    Q_OBJECT
public:
    explicit TSessiondumpHintsProcessor(std::shared_ptr<TSessiondumpHints> hints);
    ~TSessiondumpHintsProcessor() override;

    /// The current hint's timestamp; INT64_MAX when every hint is done.
    int64_t nextHintTimestamp() const;
    /// There is a current hint due at `timestamp`.
    bool hasHintToProcess(const int64_t& timestamp) const;
    /// Runs the hints due at `timestamp` and publishes their actions, up to
    /// the first one that is not done afterwards.
    void processHints(const int64_t& timestamp);

public Q_SLOTS:
    virtual void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);

private:
    /// The current hint becomes the first unfinished one.
    void updateCurrentHint();

    std::shared_ptr<TSessiondumpHints> m_hints;
    std::shared_ptr<TSessiondumpHintBase> m_currentHint;
};

/// Sibling path of a `.dmp` → `.hints` when present.
QString hintsPathForDump(const QString& dumpPath);

}
}
