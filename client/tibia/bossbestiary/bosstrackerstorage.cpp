#include "bossbestiary/bosstrackerstorage.h"

#include "cyclopedia/bosstiarystorage.h"

#include <client/staticdata.h>

#include <QDateTime>

#include <algorithm>
#include <set>

namespace tibia::bossbestiary {

namespace {

using Args = std::vector<std::any>;

} // namespace

bool TBossTrackerEntry::hasCooldown() const { return cooldownUntil > 0; }

bool TBossTrackerEntry::cooldownExpired() const
{
    return hasCooldown() && cooldownUntil <= QDateTime::currentSecsSinceEpoch();
}

TBossTrackerStorage::TBossTrackerStorage(QObject* parent)
    : cip::tibia::bossbestiary::TBossTrackerStorage(parent)
{
    m_hooks.emplace_back("g_game", "onBossCooldown", [this](LuaObject*, const Args& args) {
        const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<BossCooldownData>>(&args[0]);
        m_cooldownUntil.clear();
        if (list) {
            for (const BossCooldownData& data : *list)
                m_cooldownUntil.insert(static_cast<int>(data.bossRaceId), static_cast<qint64>(data.cooldownTime));
        }
        rebuild();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseSendBosstiary", [this](LuaObject*, const Args& args) {
        const auto* bosses = args.empty() ? nullptr : std::any_cast<std::vector<BosstiaryData>>(&args[0]);
        m_unlocked.clear();
        if (bosses) {
            for (const BosstiaryData& boss : *bosses) {
                if (boss.kills == 0)
                    continue;
                const int raceId = static_cast<int>(boss.raceId);
                m_unlocked.insert(raceId, cyclopedia::bossName(raceId));
            }
        }
        rebuild();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_cooldownUntil.clear();
        m_unlocked.clear();
        if (!m_entries.empty()) {
            m_entries.clear();
            Q_EMIT bossEntriesChanged();
        }
        return std::any{};
    });
}

void TBossTrackerStorage::rebuild()
{
    std::set<int> raceIds;
    for (auto it = m_unlocked.cbegin(); it != m_unlocked.cend(); ++it)
        raceIds.insert(it.key());
    for (auto it = m_cooldownUntil.cbegin(); it != m_cooldownUntil.cend(); ++it)
        raceIds.insert(it.key());

    std::vector<TBossTrackerEntry> next;
    next.reserve(raceIds.size());
    for (const int raceId : raceIds) {
        TBossTrackerEntry entry;
        entry.raceId = raceId;
        entry.name = m_unlocked.value(raceId);
        if (entry.name.isEmpty())
            entry.name = cyclopedia::bossName(raceId);
        entry.cooldownUntil = m_cooldownUntil.value(raceId);
        next.push_back(std::move(entry));
    }
    std::ranges::stable_sort(next, [](const TBossTrackerEntry& a, const TBossTrackerEntry& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    const bool changed = next.size() != m_entries.size()
        || !std::equal(next.begin(), next.end(), m_entries.begin(), [](const TBossTrackerEntry& a, const TBossTrackerEntry& b) {
               return a.raceId == b.raceId && a.cooldownUntil == b.cooldownUntil && a.name == b.name;
           });
    if (!changed)
        return;
    m_entries = std::move(next);
    Q_EMIT bossEntriesChanged();
}

} // namespace tibia::bossbestiary
