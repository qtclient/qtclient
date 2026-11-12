#include "bossbestiary/bosstiarystorage.h"

#include "cyclopedia/bosstiarystorage.h"

#include <client/staticdata.h>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::bossbestiary {

namespace {

using Args = std::vector<std::any>;

template <class T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

int categoryIndex(const int category) { return category == 2 ? 2 : category == 1 ? 1 : 0; }

TBossSlot makeSlot(const bool unlocked, const uint32_t bossId, const std::optional<BosstiarySlot>& data)
{
    TBossSlot slot;
    slot.unlocked = unlocked;
    if (!unlocked) {
        slot.unlockBosspoints = static_cast<int>(bossId);
        return slot;
    }
    if (bossId == 0 || !data)
        return slot;
    slot.hasBoss = true;
    slot.raceId = static_cast<int>(bossId);
    slot.name = cyclopedia::bossName(slot.raceId);
    slot.category = data->bossRace;
    slot.kills = data->killCount;
    slot.lootBonus = data->lootBonus;
    slot.killBonus = data->killBonus;
    slot.removePrice = data->removePrice;
    slot.inactive = data->inactive != 0;
    return slot;
}

} // namespace

TBosstiaryStorage::TBosstiaryStorage(QObject* parent)
    : cip::tibia::bossbestiary::TBosstiaryStorage(parent)
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBosstiaryData", [this](GameObject*, const Args& args) {
        const auto* kills = arg<std::vector<uint16_t>>(args, 0);
        const auto* points = arg<std::vector<uint16_t>>(args, 1);
        if (!kills || !points || kills->size() < 9 || points->size() < 9)
            return std::any{};
        for (int i = 0; i < 9; ++i) {
            m_killsForStage[i / 3][i % 3] = (*kills)[i];
            m_pointsForStage[i / 3][i % 3] = (*points)[i];
        }
        Q_EMIT unlockKillRequirementsChanged();
        Q_EMIT bossPointsGrantedChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseSendBosstiary", [this](GameObject*, const Args& args) {
        const auto* bosses = arg<std::vector<BosstiaryData>>(args, 0);
        if (!bosses)
            return std::any{};
        m_entries.clear();
        for (const BosstiaryData& boss : *bosses) {
            TBosstiaryEntry entry;
            entry.raceId = static_cast<int>(boss.raceId);
            entry.name = cyclopedia::bossName(entry.raceId);
            entry.category = boss.category;
            entry.kills = boss.kills;
            entry.tracked = boss.isTrackerActived != 0;
            m_entries.push_back(entry);
        }
        std::sort(m_entries.begin(), m_entries.end(), [](const TBosstiaryEntry& a, const TBosstiaryEntry& b) {
            if ((a.kills == 0) != (b.kills == 0))
                return a.kills != 0;
            return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
        });
        Q_EMIT bosstiaryEntriesChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseBosstiarySlots", [this](GameObject*, const Args& args) {
        const auto* data = arg<BosstiarySlotsData>(args, 0);
        if (!data)
            return std::any{};
        m_slots.currentBosspoints = static_cast<int>(data->playerPoints);
        m_slots.maximumBosspoints = static_cast<int>(data->totalPointsNextBonus);
        m_slots.currentLootBonus = data->currentBonus;
        m_slots.nextLootBonus = data->nextBonus;
        m_slots.first = makeSlot(data->isSlotOneUnlocked, data->bossIdSlotOne, data->slotOneData);
        m_slots.second = makeSlot(data->isSlotTwoUnlocked, data->bossIdSlotTwo, data->slotTwoData);
        m_slots.boosted = makeSlot(data->isTodaySlotUnlocked, data->boostedBossId, data->todaySlotData);
        m_slots.assignable.clear();
        for (const BossUnlocked& boss : data->bossesUnlockedData) {
            m_slots.assignable.push_back(
                { static_cast<int>(boss.bossId), boss.bossRace, cyclopedia::bossName(static_cast<int>(boss.bossId)) });
        }
        Q_EMIT bossSlotsMessageData();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBosstiaryEntryChanged", [this](GameObject*, const Args& args) {
        const int raceId = client::gamecore::argInt(args, 0);
        if (raceId <= 0)
            return std::any{};
        Q_EMIT bosstiaryEntriesChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaTracker", [this](GameObject*, const Args& args) {
        using Entry = std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>;
        if (client::gamecore::argInt(args, 0) != 1)
            return std::any{};
        const auto* entries = arg<std::vector<Entry>>(args, 1);
        if (!entries)
            return std::any{};
        m_trackedBosses.clear();
        for (const Entry& entry : *entries)
            m_trackedBosses.insert(std::get<0>(entry));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        const bool hadEntries = !m_entries.empty();
        const bool hadSlots = m_slots.currentBosspoints != 0 || m_slots.first.unlocked || m_slots.second.unlocked
            || m_slots.boosted.unlocked || !m_slots.assignable.empty();
        m_entries.clear();
        m_slots = {};
        m_trackedBosses.clear();
        if (hadEntries)
            Q_EMIT bosstiaryEntriesChanged();
        if (hadSlots)
            Q_EMIT bossSlotsMessageData();
    });
}

int TBosstiaryStorage::killsForStage(const int category, const int stage) const
{
    return stage >= 1 && stage <= 3 ? m_killsForStage[categoryIndex(category)][stage - 1] : 0;
}

int TBosstiaryStorage::pointsForStage(const int category, const int stage) const
{
    return stage >= 1 && stage <= 3 ? m_pointsForStage[categoryIndex(category)][stage - 1] : 0;
}

void TBosstiaryStorage::clearEntries()
{
    if (m_entries.empty())
        return;
    m_entries.clear();
    Q_EMIT bosstiaryEntriesChanged();
}

} // namespace tibia::bossbestiary
