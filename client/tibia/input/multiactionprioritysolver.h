#pragma once

#include "config/hotkeyoptions.h"

#include <QtGlobal>

#include <cstdint>
#include <vector>

namespace tibia {
namespace container {
class TObjectAppearanceInstanceInfoStorage;
}
namespace game {
class TCooldownStorage;
class TPlayerData;
}
namespace input {

/// CIP input::TMultiActionPrioritySolver: picks the slot of a multi action
/// that runs. The slots are asked through IAction, so the action button
/// handler (over the hotkey settings) and the action bar (over its buttons)
/// share one solver.
///
/// Host concern: CipSoft also passes the appearance storage; qtclient looks
/// object types up through the thing session.
class TMultiActionPrioritySolver
{
public:
    /// One slot as the solver asks it.
    class IAction
    {
    public:
        virtual ~IAction() = default;
        /// The slot holds nothing.
        virtual bool isEmpty() const = 0;
        /// Whether the slot casts a spell (spellId then names it).
        virtual bool resolveSpell() = 0;
        virtual uint16_t spellId() const = 0;
        /// Whether the slot uses an object.
        virtual bool hasObject() const = 0;
        virtual config::EUseType useType() const = 0;
        virtual uint16_t objectId() const = 0;
        virtual bool hasLiquidType() const = 0;
        virtual uint32_t liquidType() const = 0;
        virtual bool hasUpgradeTier() const = 0;
        virtual uint8_t upgradeTier() const = 0;
    };

    TMultiActionPrioritySolver(const game::TCooldownStorage* cooldownStorage, const game::TPlayerData* playerData,
                               const container::TObjectAppearanceInstanceInfoStorage* objectInfoStorage, qint64 now, qint64 latency);

    /// The first usable slot that is ready; else the usable slot ready
    /// soonest, a later one only when it is ready sooner by more than the
    /// latency; else the first unusable slot. Empty slots never qualify.
    IAction* solve(const std::vector<IAction*>& candidates) const;
    /// A spell needs to be known, an object to be carried; anything else is
    /// usable.
    bool isUsable(IAction& candidate) const;
    /// How long until the slot can run: the spell's cooldown, a rune's
    /// object cooldown, the multi use cooldown of an object used on something
    /// (yourself, the target, a chosen target, the mouse cursor); 0 otherwise.
    qint64 remainingDelay(IAction& candidate) const;

private:
    /// How many of the slot's object the player carries (its expire
    /// counterpart's count when that is higher).
    int objectCount(IAction& candidate) const;

    const game::TCooldownStorage* m_cooldownStorage;
    const game::TPlayerData* m_playerData;
    const container::TObjectAppearanceInstanceInfoStorage* m_objectInfoStorage;
    qint64 m_now;
    qint64 m_latency;
};

}
}
