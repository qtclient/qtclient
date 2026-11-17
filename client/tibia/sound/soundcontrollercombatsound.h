#pragma once

#include "sound/soundcontrollerbase.h"

#include <client/position.h>

#include <vector>

namespace tibia {
namespace sound {

/// A sound effect of the game world (CIP: TSoundStorage's sound to play).
struct TSoundToPlay
{
    TSoundEffectID effectId = 0;
    Position position;
    /// 0 none, 1 own, 2 other players, 3 creatures.
    int source = 0;
    /// When it plays (TSoundStorage's queue).
    int64_t timestampMs = 0;
};

/// CIP sound::TSoundControllerCombatSound: the world's one-shot effects,
/// quieter with distance from the player.
class TSoundControllerCombatSound final : public TSoundControllerBase
{
    Q_OBJECT
public:
    using TSoundControllerBase::TSoundControllerBase;

    void setPlayerPosition(const Position& position) { m_playerPosition = position; }
    void playEffectSounds(const std::vector<TSoundToPlay>& sounds);
    /// Stops every single shot.
    void stopAll();

private:
    Position m_playerPosition;
};

}
}
