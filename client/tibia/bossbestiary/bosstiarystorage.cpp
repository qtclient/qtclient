#include "bossbestiary/bosstiarystorage.h"

#include <algorithm>

namespace tibia::bossbestiary {

namespace {

/// Nemesis, archfoe, anything else bane.
int categoryIndex(const int category) { return category == 2 ? 2 : category == 1 ? 1 : 0; }

}

TBosstiaryStorage::TBosstiaryStorage(QObject* parent)
    : cip::tibia::bossbestiary::TBosstiaryStorage(parent)
{
}

int TBosstiaryStorage::killsForStage(const int category, const int stage) const
{
    return stage >= 1 && stage <= 3 ? m_killsForStage[categoryIndex(category)][stage - 1] : 0;
}

int TBosstiaryStorage::pointsForStage(const int category, const int stage) const
{
    return stage >= 1 && stage <= 3 ? m_pointsForStage[categoryIndex(category)][stage - 1] : 0;
}

QString TBosstiaryStorage::rarityTooltip(const int category) const
{
    const char* name = category == 2 ? "bosstiary_nemesis" : category == 1 ? "bosstiary_archfoe" : "bosstiary_bane";
    return qtTrId("bosstiary_rarity_tooltip")
        .arg(qtTrId(name))
        .arg(pointsForStage(category, 1))
        .arg(pointsForStage(category, 2))
        .arg(pointsForStage(category, 3));
}

void TBosstiaryStorage::setKillsForStage(const TBossStageValues& kills)
{
    if (kills == m_killsForStage)
        return;
    m_killsForStage = kills;
    Q_EMIT unlockKillRequirementsChanged();
}

void TBosstiaryStorage::setPointsForStage(const TBossStageValues& points)
{
    if (points == m_pointsForStage)
        return;
    m_pointsForStage = points;
    Q_EMIT bossPointsGrantedChanged();
}

void TBosstiaryStorage::setEntries(std::vector<TBosstiaryEntry> entries)
{
    m_entries = std::move(entries);
    std::sort(m_entries.begin(), m_entries.end(), [](const TBosstiaryEntry& a, const TBosstiaryEntry& b) {
        if ((a.kills == 0) != (b.kills == 0))
            return a.kills != 0;
        return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
    });
    Q_EMIT bosstiaryEntriesChanged();
}

void TBosstiaryStorage::setFilter(const TBosstiaryFilter& filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    Q_EMIT bosstiaryEntriesChanged();
}

/// The search needs a boss already killed whose name holds it; the switches
/// drop a rarity, the unkilled bosses or a kill stage.
bool TBosstiaryStorage::accepts(const TBosstiaryEntry& entry) const
{
    const qint64 kills = entry.kills;
    if (!m_filter.search.isEmpty() && (kills == 0 || !entry.name.contains(m_filter.search, Qt::CaseInsensitive)))
        return false;
    if ((!m_filter.showBane && entry.category == 0) || (!m_filter.showArchfoe && entry.category == 1)
        || (!m_filter.showNemesis && entry.category == 2) || (!m_filter.showNoKill && kills == 0))
        return false;
    const int prowess = killsForStage(entry.category, 1);
    const int expertise = killsForStage(entry.category, 2);
    const int mastery = killsForStage(entry.category, 3);
    if (!m_filter.showFewKills && kills >= 1 && kills < prowess)
        return false;
    if (!m_filter.showProwess && kills >= prowess && kills < expertise)
        return false;
    if (!m_filter.showExpertise && kills >= expertise && kills < mastery)
        return false;
    return m_filter.showMastery || kills < mastery;
}

std::vector<TBosstiaryEntry> TBosstiaryStorage::filteredEntries() const
{
    std::vector<TBosstiaryEntry> filtered;
    for (const TBosstiaryEntry& entry : m_entries) {
        if (accepts(entry))
            filtered.push_back(entry);
    }
    return filtered;
}

int TBosstiaryStorage::filteredEntryCount() const
{
    return static_cast<int>(std::ranges::count_if(m_entries, [this](const TBosstiaryEntry& entry) { return accepts(entry); }));
}

void TBosstiaryStorage::clearEntries()
{
    m_entries.clear();
    m_filter = {};
}

void TBosstiaryStorage::setBossSlots(TBossSlotsData slots)
{
    if (slots == m_slots)
        return;
    m_slots = std::move(slots);
    Q_EMIT bossSlotsMessageData();
}

} // namespace tibia::bossbestiary
