#include "bosstiarystorage.h"

#include <client/staticdata.h>
#include <tibia/assets/things.h>

#include <QStringList>

#include <algorithm>
#include <tuple>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

template <class T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

/// CIP FUN_100986240 / FUN_100986420: nemesis, archfoe, anything else bane.
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
    slot.name = bossName(slot.raceId);
    slot.category = data->bossRace;
    slot.kills = data->killCount;
    slot.lootBonus = data->lootBonus;
    slot.killBonus = data->killBonus;
    slot.removePrice = data->removePrice;
    slot.inactive = data->inactive != 0;
    return slot;
}

}

QString bossName(const int raceId)
{
    const QString name = QString::fromStdString(g_things.getRaceData(static_cast<uint32_t>(raceId)).name);
    QStringList words = name.split(QStringLiteral(" "));
    for (QString& word : words) {
        if (!word.isEmpty())
            word = word.at(0).toUpper() + word.mid(1);
    }
    return words.join(QStringLiteral(" "));
}

QString bossRaritySource(const int category)
{
    switch (category) {
        case 0: return QStringLiteral("/images/icon-bosstiary-bane.png");
        case 1: return QStringLiteral("/images/icon-bosstiary-archfoe.png");
        case 2: return QStringLiteral("/images/icon-bosstiary-nemesis.png");
        default: return {};
    }
}

TBosstiaryStorage::TBosstiaryStorage(QObject* parent)
    : QObject(parent)
{
    m_hooks.emplace_back("g_game", "onBosstiaryData", [this](LuaObject*, const Args& args) {
        const auto* kills = arg<std::vector<uint16_t>>(args, 0);
        const auto* points = arg<std::vector<uint16_t>>(args, 1);
        if (!kills || !points || kills->size() < 9 || points->size() < 9)
            return std::any{};
        for (int i = 0; i < 9; ++i) {
            m_killsForStage[i / 3][i % 3] = (*kills)[i];
            m_pointsForStage[i / 3][i % 3] = (*points)[i];
        }
        Q_EMIT killStagesChanged();
        Q_EMIT pointsChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseSendBosstiary", [this](LuaObject*, const Args& args) {
        const auto* bosses = arg<std::vector<BosstiaryData>>(args, 0);
        if (!bosses)
            return std::any{};
        m_entries.clear();
        for (const BosstiaryData& boss : *bosses) {
            TBosstiaryEntry entry;
            entry.raceId = static_cast<int>(boss.raceId);
            entry.name = bossName(entry.raceId);
            entry.category = boss.category;
            entry.kills = boss.kills;
            entry.tracked = boss.isTrackerActived != 0;
            m_entries.push_back(entry);
        }
        // CIP FUN_100986b50: the killed bosses first, each half by name.
        std::sort(m_entries.begin(), m_entries.end(), [](const TBosstiaryEntry& a, const TBosstiaryEntry& b) {
            if ((a.kills == 0) != (b.kills == 0))
                return a.kills != 0;
            return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
        });
        Q_EMIT entriesChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseBosstiarySlots", [this](LuaObject*, const Args& args) {
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
        for (const BossUnlocked& boss : data->bossesUnlockedData)
            m_slots.assignable.push_back({ static_cast<int>(boss.bossId), boss.bossRace, bossName(static_cast<int>(boss.bossId)) });
        Q_EMIT slotsChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onBosstiaryEntryChanged", [this](LuaObject*, const Args& args) {
        const int raceId = client::gamecore::argInt(args, 0);
        if (raceId <= 0)
            return std::any{};
        // Soft: an entry changed — refresh tracker notify so Soft bosstiary panels re-read.
        Q_EMIT trackerChanged();
        Q_EMIT entriesChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseCyclopediaTracker", [this](LuaObject*, const Args& args) {
        using Entry = std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>;
        if (client::gamecore::argInt(args, 0) != 1)
            return std::any{};
        const auto* entries = arg<std::vector<Entry>>(args, 1);
        if (!entries)
            return std::any{};
        m_trackedBosses.clear();
        for (const Entry& entry : *entries)
            m_trackedBosses.insert(std::get<0>(entry));
        Q_EMIT trackerChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_entries.clear();
        m_slots = {};
        m_trackedBosses.clear();
        return std::any{};
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

QString TBosstiaryStorage::rarityTooltip(const int category) const
{
    const char* name = category == 2 ? "bosstiary_nemesis" : category == 1 ? "bosstiary_archfoe" : "bosstiary_bane";
    return qtTrId("bosstiary_rarity_tooltip")
        .arg(qtTrId(name))
        .arg(pointsForStage(category, 1))
        .arg(pointsForStage(category, 2))
        .arg(pointsForStage(category, 3));
}

void TBosstiaryStorage::clearEntries()
{
    m_entries.clear();
    Q_EMIT entriesChanged();
}

}
