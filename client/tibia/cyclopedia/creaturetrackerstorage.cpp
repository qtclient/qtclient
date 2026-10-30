#include "cyclopedia/creaturetrackerstorage.h"

#include "cyclopedia/bosstiarystorage.h"

#include <tibia/assets/things.h>

#include <algorithm>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;
using PacketEntry = std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>;

QString raceDisplayName(const int raceId, const bool boss)
{
    if (boss)
        return bossName(raceId);
    QString name = QString::fromStdString(g_things.getRaceData(static_cast<uint32_t>(raceId)).name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

std::vector<TCreatureTrackerEntry> fromPacket(const std::vector<PacketEntry>& packet, const bool boss)
{
    std::vector<TCreatureTrackerEntry> out;
    out.reserve(packet.size());
    for (const PacketEntry& row : packet) {
        TCreatureTrackerEntry entry;
        entry.raceId = std::get<0>(row);
        entry.currentKills = std::get<1>(row);
        entry.killsForDetails1 = std::get<2>(row);
        entry.killsForDetails2 = std::get<3>(row);
        entry.killsForDetails3 = std::get<4>(row);
        entry.status = std::get<5>(row);
        entry.name = raceDisplayName(entry.raceId, boss);
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace

TCreatureTrackerStorage::TCreatureTrackerStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCreatureTrackerStorage(parent)
{
    m_hooks.emplace_back("g_game", "onParseCyclopediaTracker", [this](LuaObject*, const Args& args) {
        const int trackerType = client::gamecore::argInt(args, 0);
        const auto* packet = args.size() > 1 ? std::any_cast<std::vector<PacketEntry>>(&args[1]) : nullptr;
        if (!packet)
            return std::any{};
        applyTracker(trackerType, fromPacket(*packet, trackerType == 1));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        if (!m_monsters.empty()) {
            m_monsters.clear();
            Q_EMIT trackedMonstersChanged();
            Q_EMIT canTrackMoreMonstersChanged();
        }
        if (!m_bosses.empty()) {
            m_bosses.clear();
            Q_EMIT trackedBossesChanged();
            Q_EMIT canTrackMoreBossesChanged();
        }
        return std::any{};
    });
}

void TCreatureTrackerStorage::applyTracker(const int trackerType, const std::vector<TCreatureTrackerEntry>& entries)
{
    if (trackerType == 1) {
        const bool canMore = static_cast<int>(entries.size()) < TrackLimit;
        const bool canChanged = canMore != canTrackMoreBosses();
        m_bosses = entries;
        Q_EMIT trackedBossesChanged();
        if (canChanged)
            Q_EMIT canTrackMoreBossesChanged();
        return;
    }
    if (trackerType != 0)
        return;
    const bool canMore = static_cast<int>(entries.size()) < TrackLimit;
    const bool canChanged = canMore != canTrackMoreMonsters();
    m_monsters = entries;
    Q_EMIT trackedMonstersChanged();
    if (canChanged)
        Q_EMIT canTrackMoreMonstersChanged();
}

} // namespace tibia::cyclopedia
