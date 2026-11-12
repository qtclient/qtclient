#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <map>
#include <vector>

namespace tibia {
namespace game {

/// A running cooldown: its length and when it ends, in TCooldownStorage::now
/// milliseconds. A zero end is no cooldown.
struct TCooldown
{
    qint64 duration = 0;
    qint64 end = 0;
};

/// The spell, spell group, multi-use and passive-ability cooldowns the server
/// announced (CIP: TCooldownStorage, ctor , made by TGameSessionBase
/// v149).
class TCooldownStorage final : public cip::tibia::game::TCooldownStorage
{
    Q_OBJECT
public:
    explicit TCooldownStorage(QObject* parent = nullptr);
    ~TCooldownStorage() override;

    static TCooldownStorage* instance();
    /// Monotonic milliseconds.
    static qint64 now();

    /// The spell's own cooldown or the longer of its groups' (CIP: vtable 0xb8,
    /// read by TActionBarController).
    TCooldown spellCooldown(int spellId) const;
    /// The group's own cooldown only (CIP cooldown bar).
    TCooldown groupCooldown(int groupId) const;
    const std::map<int, TCooldown>& spellCooldowns() const { return m_spells; }
    const std::map<int, TCooldown>& groupCooldowns() const { return m_groups; }
    /// CIP vtable 0xc0.
    TCooldown multiUseCooldown() const { return m_multiUse; }
    /// CIP PassiveAbilityData (S 0x5E): Gift of Life / wheel passives.
    /// `duration` is total; `end` is now+remaining (paused when !canDecay).
    TCooldown passiveCooldown() const;
    int passiveAbilityId() const { return m_passiveAbilityId; }
    bool anyRunning() const;

    /// CIP protocol → storage (prefer over g_game onSpellCooldown publish).
    void applySpellCooldown(int spellId, qint64 durationMs);
    void applySpellGroupCooldown(int groupId, qint64 durationMs);
    void applyMultiUseCooldown(qint64 durationMs);
    void applyPassiveAbilityData(int abilityId, qint64 remainingMs, qint64 totalMs, bool decaying);
    void applyGameEnd();

Q_SIGNALS:
    /// qtclient extension beyond CIP moc (wheel / Gift of Life overlays).
    void passiveCooldownChanged();

private:
    void clear();

    std::map<int, TCooldown> m_spells;
    std::map<int, TCooldown> m_groups;
    TCooldown m_multiUse;
    int m_passiveAbilityId = 0;
    qint64 m_passiveRemaining = 0;
    qint64 m_passiveDuration = 0;
    qint64 m_passiveRecvAt = 0;
    bool m_passiveDecaying = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
