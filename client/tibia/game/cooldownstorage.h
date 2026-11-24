#pragma once

#include <cip_gen.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace tibia {
namespace game {

/// A cooldown as the storage hands it out: time left, its whole length (at
/// least 1 while running) and when it ends, in milliseconds of the game
/// session's clock. All zero is no cooldown.
struct TCooldown
{
    qint64 remaining = 0;
    qint64 duration = 0;
    qint64 end = 0;
};

/// One running spell cooldown of the cooldown bar's spell list.
struct TSpellCooldownEntry
{
    uint16_t spellId = 0;
    qint64 remaining = 0;
    qint64 duration = 0;
};

/// One spell group of the cooldown bar's group row.
struct TSpellGroupCooldownEntry
{
    uint8_t groupId = 0;
    qint64 remaining = 0;
    qint64 duration = 0;
};

/// The server's passive ability cooldown: the two values it sent, when it
/// arrived (wall clock milliseconds) and whether it keeps counting down.
struct TPassiveAbilityCooldown
{
    qint64 remaining = 0;
    qint64 total = 0;
    qint64 receivedAt = 0;
    bool decaying = false;

    /// The time left at `now`: counting down from when it arrived while it
    /// decays (not below 0), else as sent.
    qint64 remainingAt(const qint64 now) const
    {
        return decaying ? std::max<qint64>(0, receivedAt - now + remaining) : remaining;
    }
    /// The part of the whole length left at `now`, 0 to 1 (0 without a length).
    double fractionAt(const qint64 now) const
    {
        if (total == 0)
            return 0.0;
        const double left = std::max(0.0, double(remainingAt(now))) / double(total);
        return left < 0.0 ? 0.0 : std::min(1.0, left);
    }
    bool operator==(const TPassiveAbilityCooldown&) const = default;
};

/// The server's passive ability counter.
struct TPassiveAbilityCounter
{
    uint32_t current = 0;
    uint32_t maximum = 0;

    /// What is left to count.
    int remaining() const { return int(maximum) - int(current); }
    /// The part left, 0 without a maximum.
    double fraction() const { return maximum == 0 ? 0.0 : double(int(maximum) - int(current)) / double(int(maximum)); }
    bool operator==(const TPassiveAbilityCounter&) const = default;
};

/// The spell, spell group, multi-use and passive ability cooldowns the server
/// announced, kept as end times on the game session's clock: its callers
/// pass the time.
class TCooldownStorage final : public cip::tibia::game::TCooldownStorage
{
    Q_OBJECT
public:
    explicit TCooldownStorage(QObject* parent = nullptr);
    ~TCooldownStorage() override;

    /// Sets a known spell's end time; an unknown spell that already has one
    /// keeps it.
    void updateSpellCooldownEnd(uint16_t spellId, qint64 end);
    void setSpellCooldownEnd(uint16_t spellId, qint64 end);
    void setSpellGroupCooldown(uint8_t groupId, qint64 start, qint64 end);
    void setMultiUseCooldown(qint64 start, qint64 end);
    void setPassiveAbilityCooldown(uint8_t abilityId, qint64 remaining, qint64 total, bool decaying);
    void setPassiveAbilityCounter(uint8_t abilityId, uint32_t current, uint32_t maximum);
    /// Drops the cooldowns that ended before `time`.
    void expireCooldowns(qint64 time);

    std::vector<TSpellCooldownEntry> spellCooldownList(qint64 time) const;
    std::vector<TSpellGroupCooldownEntry> spellGroupCooldownList(qint64 time) const;
    /// The spell's own cooldown, without its groups.
    TCooldown spellOwnCooldown(uint16_t spellId, qint64 time) const;
    TCooldown spellGroupCooldown(uint8_t groupId, qint64 time) const;
    /// The spell's own cooldown joined with its two groups'.
    TCooldown spellCooldown(uint16_t spellId, qint64 time) const;
    TCooldown multiUseCooldown(qint64 time) const;
    /// The multi-use cooldown joined with the object's rune spell.
    TCooldown objectCooldown(uint16_t objectId, qint64 time) const;

    /// Spell id → own cooldown at `time` of every spell with an end time.
    std::map<int, TCooldown> spellCooldowns(qint64 time) const;
    /// The server's cooldown and counter of a passive ability the client
    /// knows; all zero for one without data or an unknown id.
    TPassiveAbilityCooldown passiveAbilityCooldown(uint8_t abilityId) const;
    TPassiveAbilityCounter passiveAbilityCounter(uint8_t abilityId) const;
    /// The cooldown of the first passive ability with data (Gift of Life),
    /// ending on the clock of `time`.
    TCooldown passiveCooldown(qint64 time) const;
    int passiveAbilityId() const;

private:
    std::map<uint16_t, qint64> m_spellCooldownEnds;
    std::map<uint8_t, qint64> m_spellGroupCooldownStarts;
    std::map<uint8_t, qint64> m_spellGroupCooldownEnds;
    qint64 m_multiUseCooldownStart = 0;
    qint64 m_multiUseCooldownEnd = 0;
    std::map<uint8_t, TPassiveAbilityCooldown> m_passiveAbilityCooldowns;
    std::map<uint8_t, TPassiveAbilityCounter> m_passiveAbilityCounters;
};

}
}
