#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <map>
#include <vector>

namespace tibia::game {

/// A running cooldown: its length and when it ends, in TCooldownStorage::now()
/// milliseconds. A zero end is no cooldown.
struct TCooldown
{
    qint64 duration = 0;
    qint64 end = 0;
};

/// The spell, spell group and multi-use cooldowns the server announced (CIP:
/// TCooldownStorage, ctor 0x100b42cc0, made by TGameSessionBase v149
/// FUN_100b1ef40). Passive ability cooldowns are not ported.
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
    /// read by TActionBarController FUN_100c80300).
    TCooldown spellCooldown(int spellId) const;
    /// The group's own cooldown only (CIP cooldown bar / FUN_100c0c0f0).
    TCooldown groupCooldown(int groupId) const;
    const std::map<int, TCooldown>& spellCooldowns() const { return m_spells; }
    const std::map<int, TCooldown>& groupCooldowns() const { return m_groups; }
    /// CIP vtable 0xc0.
    TCooldown multiUseCooldown() const { return m_multiUse; }
    bool anyRunning() const;

private:
    void clear();

    std::map<int, TCooldown> m_spells;
    std::map<int, TCooldown> m_groups;
    TCooldown m_multiUse;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
