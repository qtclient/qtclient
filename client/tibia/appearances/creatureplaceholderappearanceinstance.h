#pragma once

#include "appearances/appearanceinstance.h"

#include <memory>

namespace tibia {
namespace creatures {
class TCreature;
} // namespace creatures

namespace appearances {

/// What a creature leaves on the map: an appearance instance that stands for
/// the creature it belongs to and keeps the state deciding whether its moving
/// sprites show. Every creature owns one.
class TCreaturePlaceholderAppearanceInstance final : public TAppearanceInstance
{
public:
    /// The placeholder of `creature`, drawing the empty creature appearance.
    explicit TCreaturePlaceholderAppearanceInstance(std::weak_ptr<creatures::TCreature> creature);

    /// CIP v5: updates the creature to `nowMs` and remembers the time; while the
    /// creature moves it also remembers it as the last moving time and lets
    /// the moving sprites show again. Always reports a change.
    bool updateAnimation(qint64 nowMs) override;
    /// CIP v9: the creature's offset within its step, none without a creature.
    worldmap::TWorldMapSubfieldExtent movementOffset() const override;
    /// CIP v10.
    bool isCreature() const override { return true; }
    /// CIP v13: the creature, null once it is gone.
    std::shared_ptr<creatures::TCreature> creature() const { return m_creature.lock(); }
    /// CIP v15: the displacement the creature is drawn with: its mount's while
    /// it rides, its outfit's otherwise, with its draw elevation as depth.
    worldmap::TWorldMapSubfieldExtent creatureDisplacement() const;
    /// CIP v16: the local player holds a movement intent other than stop; the
    /// moving sprites then stay shown briefly between steps.
    void setHoldsMovementIntent(bool holds) { m_holdsMovementIntent = holds; }
    /// CIP v17: the moving sprites stay hidden until the creature moves again.
    void setMovingSpriteSuppressed(bool suppressed) { m_movingSpriteSuppressed = suppressed; }
    /// The creature shows its moving sprites: another creature while it moves;
    /// the local player while it moves or moved at most 75 ms before the last
    /// update and holds a movement intent, unless they are suppressed.
    bool showsMovingSprite() const;

private:
    std::weak_ptr<creatures::TCreature> m_creature;
    bool m_holdsMovementIntent = false;
    bool m_movingSpriteSuppressed = false;
    qint64 m_lastUpdateMs = 0;
    qint64 m_lastMovingMs = 0;
};

} // namespace appearances
} // namespace tibia
