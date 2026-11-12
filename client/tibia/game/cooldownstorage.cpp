#include "cooldownstorage.h"

#include "spells/spellinfostorage.h"
#include "game/tgamenotify.h"

#include <QElapsedTimer>
#include <tibia/game/gamenotify.h>

namespace tibia::game {

namespace {

using Args = std::vector<std::any>;

TCooldownStorage* g_instance = nullptr;

TCooldown started(const qint64 duration)
{
    return { duration, TCooldownStorage::now() + duration };
}

}

TCooldownStorage::TCooldownStorage(QObject* parent)
    : cip::tibia::game::TCooldownStorage(parent)
{
    g_instance = this;
    // Keep-alive hooks for any residual publishers; ProtocolGame prefers apply*.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSpellCooldown", [this](GameObject*, const Args& args) {
        applySpellCooldown(int(client::gamecore::argInt(args, 0)), client::gamecore::argInt(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSpellGroupCooldown", [this](GameObject*, const Args& args) {
        applySpellGroupCooldown(int(client::gamecore::argInt(args, 0)), client::gamecore::argInt(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMultiUseCooldown", [this](GameObject*, const Args& args) {
        applyMultiUseCooldown(client::gamecore::argInt(args, 0));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPassiveData", [this](GameObject*, const Args& args) {
        applyPassiveAbilityData(int(client::gamecore::argInt(args, 0)), client::gamecore::argInt(args, 1),
                                client::gamecore::argInt(args, 2), client::gamecore::argBool(args, 3));
        return std::any{};
    });
    connect(TGameNotify::instance(), &TGameNotify::gameEnded, this, [this] { applyGameEnd(); });
}

void TCooldownStorage::applySpellCooldown(const int spellId, const qint64 durationMs)
{
    m_spells[spellId] = started(durationMs);
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::applySpellGroupCooldown(const int groupId, const qint64 durationMs)
{
    m_groups[groupId] = started(durationMs);
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::applyMultiUseCooldown(const qint64 durationMs)
{
    m_multiUse = started(durationMs);
    Q_EMIT multiUseCooldownChanged();
}

void TCooldownStorage::applyPassiveAbilityData(const int abilityId, const qint64 remainingMs, const qint64 totalMs,
                                             const bool decaying)
{
    m_passiveAbilityId = abilityId;
    m_passiveRemaining = remainingMs;
    m_passiveDuration = totalMs;
    m_passiveDecaying = decaying;
    m_passiveRecvAt = now();
    Q_EMIT passiveCooldownChanged();
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::applyGameEnd()
{
    clear();
}

TCooldownStorage::~TCooldownStorage()
{
    m_hooks.clear();
    if (g_instance == this)
        g_instance = nullptr;
}

TCooldownStorage* TCooldownStorage::instance()
{
    return g_instance;
}

qint64 TCooldownStorage::now()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer.elapsed();
}

TCooldown TCooldownStorage::spellCooldown(const int spellId) const
{
    TCooldown longest;
    const auto consider = [&longest](const std::map<int, TCooldown>& map, const int key) {
        if (const auto it = map.find(key); it != map.end() && it->second.end > longest.end)
            longest = it->second;
    };
    consider(m_spells, spellId);
    if (const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId)) {
        if (spell->groupPrimary)
            consider(m_groups, spell->groupPrimary);
        if (spell->groupSecondary)
            consider(m_groups, spell->groupSecondary);
    }
    return longest;
}

TCooldown TCooldownStorage::groupCooldown(const int groupId) const
{
    if (const auto it = m_groups.find(groupId); it != m_groups.end())
        return it->second;
    return {};
}

TCooldown TCooldownStorage::passiveCooldown() const
{
    if (m_passiveDuration <= 0 && m_passiveRemaining <= 0)
        return {};
    qint64 remaining = m_passiveRemaining;
    if (m_passiveDecaying)
        remaining = std::max<qint64>(0, m_passiveRemaining - (now() - m_passiveRecvAt));
    if (remaining <= 0 && m_passiveDuration <= 0)
        return {};
    return { std::max(m_passiveDuration, remaining), now() + remaining };
}

bool TCooldownStorage::anyRunning() const
{
    const qint64 time = now();
    if (m_multiUse.end > time)
        return true;
    if (passiveCooldown().end > time)
        return true;
    for (const auto* map : { &m_spells, &m_groups })
        for (const auto& [id, cooldown] : *map)
            if (cooldown.end > time)
                return true;
    return false;
}

void TCooldownStorage::clear()
{
    m_spells.clear();
    m_groups.clear();
    m_multiUse = {};
    m_passiveAbilityId = 0;
    m_passiveRemaining = 0;
    m_passiveDuration = 0;
    m_passiveRecvAt = 0;
    m_passiveDecaying = false;
    Q_EMIT spellOrSpellGroupCooldownChanged();
    Q_EMIT multiUseCooldownChanged();
    Q_EMIT passiveCooldownChanged();
}

}
