#include "cooldownstorage.h"

#include "spells/spellinfostorage.h"

#include <QElapsedTimer>

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
    m_hooks.emplace_back("g_game", "onSpellCooldown", [this](LuaObject*, const Args& args) {
        m_spells[int(client::gamecore::argInt(args, 0))] = started(client::gamecore::argInt(args, 1));
        Q_EMIT spellOrSpellGroupCooldownChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onSpellGroupCooldown", [this](LuaObject*, const Args& args) {
        m_groups[int(client::gamecore::argInt(args, 0))] = started(client::gamecore::argInt(args, 1));
        Q_EMIT spellOrSpellGroupCooldownChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onMultiUseCooldown", [this](LuaObject*, const Args& args) {
        m_multiUse = started(client::gamecore::argInt(args, 0));
        Q_EMIT multiUseCooldownChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        clear();
        return std::any{};
    });
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

bool TCooldownStorage::anyRunning() const
{
    const qint64 time = now();
    if (m_multiUse.end > time)
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
    Q_EMIT spellOrSpellGroupCooldownChanged();
    Q_EMIT multiUseCooldownChanged();
}

}
