#pragma once

#include <cip_gen.h>

#include <cstdint>
#include <map>
#include <vector>

namespace tibia {
namespace game {

/// A cooldown as the storage hands it out: time left, its whole length (at
/// least 1 while running) and when it ends, in TCooldownStorage::now
/// milliseconds. All zero is no cooldown.
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
/// arrived and whether it keeps counting down.
struct TPassiveAbilityCooldown
{
    qint64 remaining = 0;
    qint64 total = 0;
    qint64 receivedAt = 0;
    bool decaying = false;
};

/// The server's passive ability counter.
struct TPassiveAbilityCounter
{
    uint32_t current = 0;
    uint32_t maximum = 0;
};

/// The spell, spell group, multi-use and passive ability cooldowns the server
/// announced, kept as end times.
class TCooldownStorage final : public cip::tibia::game::TCooldownStorage
{
    Q_OBJECT
public:
    explicit TCooldownStorage(QObject* parent = nullptr);
    ~TCooldownStorage() override;

    /// Host shim for the IdleOT pack: the current game session's storage
    /// (each session owns one; the game client sets it).
    static TCooldownStorage* instance();
    static void setInstance(TCooldownStorage* storage);
    /// Monotonic milliseconds; every end time is on this clock.
    static qint64 now();

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

    /// Callers on the host clock.
    TCooldown spellCooldown(const int spellId) const { return spellCooldown(uint16_t(spellId), now()); }
    TCooldown groupCooldown(const int groupId) const { return spellGroupCooldown(uint8_t(groupId), now()); }
    TCooldown multiUseCooldown() const { return multiUseCooldown(now()); }
    /// Spell id → own cooldown of every spell with an end time.
    std::map<int, TCooldown> spellCooldowns() const;
    /// The cooldown of the first passive ability with data (Gift of Life).
    TCooldown passiveCooldown() const;
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
