#include "cyclopediamapdata.h"
#include "game/gamesessionbase.h"
#include <algorithm>

#include "client/gameclient.h"
#include "client/gamecore.h"
#include "creatures/creaturestorage.h"


#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::cyclopedia {

TCyclopediaMapStorage::TCyclopediaMapStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCyclopediaMapStorage(parent)
    , m_playerPositionElement{ worldmap::TWorldMapCuboid(worldmap::TWorldMapCoordinate(0, 0, 0), worldmap::TWorldMapExtent(0, 0, 1)),
                               worldmap::TPlayerPositionMapAreaData{} }
{
    using client::gamecore::argInt;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapDiscoveryData", [this](GameObject*, const std::vector<std::any>& args) {
        const auto* mains = args.size() > 0 ? std::any_cast<std::vector<std::vector<int>>>(&args[0]) : nullptr;
        const auto* discovered = args.size() > 1 ? std::any_cast<std::vector<int>>(&args[1]) : nullptr;
        const auto* discoverable = args.size() > 2 ? std::any_cast<std::vector<int>>(&args[2]) : nullptr;
        m_mainAreas.clear();
        if (mains) {
            for (const std::vector<int>& main : *mains) {
                if (main.size() >= 3)
                    m_mainAreas[main[0]] = { main[1], static_cast<float>(main[2]) / 100.0f };
            }
        }
        m_discovered = discovered ? std::set<int>(discovered->begin(), discovered->end()) : std::set<int>();
        m_discoverable = discoverable ? std::set<int>(discoverable->begin(), discoverable->end()) : std::set<int>();
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT changed();
        return std::any{};
    });
    // CIP : an active area replaces the viewpoints (full session list + states).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapDiscoveryArea", [this](GameObject*, const std::vector<std::any>& args) {
        // Every SetDiscoveryArea, active or not, tells the subarea's viewpoint counts.
        const auto* areaPoints = args.size() > 3 ? std::any_cast<std::vector<std::vector<int>>>(&args[3]) : nullptr;
        int found = 0;
        if (areaPoints) {
            for (const std::vector<int>& point : *areaPoints)
                found += point.size() >= 4 && point[3] != 0 ? 1 : 0;
        }
        setSubAreaViewpointInfo(static_cast<quint16>(argInt(args, 0)), static_cast<quint8>(argInt(args, 2)),
                                static_cast<quint8>(found));
        if (!argInt(args, 1)) {
            Q_EMIT areaAndSubAreaDataChanged();
            Q_EMIT changed();
            return std::any{};
        }
        m_discoverySubarea = static_cast<int>(argInt(args, 0));
        m_poiTarget = static_cast<int>(argInt(args, 2));
        m_viewpoints.clear();
        if (const auto* points = args.size() > 3 ? std::any_cast<std::vector<std::vector<int>>>(&args[3]) : nullptr) {
            for (const std::vector<int>& point : *points) {
                if (point.size() >= 4)
                    m_viewpoints.push_back({ { point[0], point[1], point[2] }, point[3] });
            }
        }
        if (m_poiTarget <= 0)
            m_poiTarget = static_cast<int>(m_viewpoints.size());
        Q_EMIT viewpointsChanged();
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT changed();
        return std::any{};
    });
    // CIP sends one passage per message; a passage at a known coordinate replaces it.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapPassage", [this](GameObject*, const std::vector<std::any>& args) {
        const auto* row = args.size() > 0 ? std::any_cast<std::vector<int>>(&args[0]) : nullptr;
        if (!row || row->size() < 4)
            return std::any{};
        TPassage passage;
        passage.coordinate = { (*row)[0], (*row)[1], (*row)[2] };
        const int subAreaCount = (*row)[3];
        for (int i = 0; i < subAreaCount && 4 + i < static_cast<int>(row->size()); ++i)
            passage.subAreaIds.push_back((*row)[4 + i]);
        const auto it = std::ranges::find_if(m_passages, [&](const TPassage& p) {
            return p.coordinate.x == passage.coordinate.x && p.coordinate.y == passage.coordinate.y && p.coordinate.z == passage.coordinate.z;
        });
        if (it != m_passages.end())
            *it = std::move(passage);
        else
            m_passages.push_back(std::move(passage));
        Q_EMIT passagesChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapSubAreaMonsters", [this](GameObject*, const std::vector<std::any>& args) {
        const auto* areas = args.size() > 0 ? std::any_cast<std::vector<std::vector<int>>>(&args[0]) : nullptr;
        if (!areas)
            return std::any{};
        for (const std::vector<int>& row : *areas) {
            if (row.empty())
                continue;
            std::vector<TSubAreaMonster> monsters;
            for (size_t i = 1; i + 2 < row.size(); i += 3) {
                const auto raceId = static_cast<quint16>(row[i]);
                monsters.push_back({ raceId, row[i + 2] });
                setRaceKnown(raceId, row[i + 1] != 0);
            }
            setSubAreaMonsters(static_cast<quint16>(row[0]), std::move(monsters));
        }
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapMonsterBestiary", [this](GameObject*, const std::vector<std::any>& args) {
        setRaceKnown(static_cast<quint16>(argInt(args, 0)), argInt(args, 1) != 0);
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapDonations", [this](GameObject*, const std::vector<std::any>& args) {
        if (const auto* goal = args.size() > 0 ? std::any_cast<uint64_t>(&args[0]) : nullptr)
            m_donationGoal = *goal;
        m_donations.clear();
        if (const auto* areas = args.size() > 1 ? std::any_cast<std::vector<std::vector<double>>>(&args[1]) : nullptr) {
            for (const std::vector<double>& area : *areas) {
                if (area.size() >= 3)
                    m_donations[static_cast<int>(area[0])] = { area[1] != 0, static_cast<quint64>(area[2]) };
            }
        }
        Q_EMIT areaDonationsChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapCurrentArea", [this](GameObject*, const std::vector<std::any>& args) {
        m_currentArea = static_cast<int>(argInt(args, 0));
        Q_EMIT playerAreaChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapExploringArea", [this](GameObject*, const std::vector<std::any>& args) {
        m_exploringSubarea = static_cast<int>(argInt(args, 0));
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT changed();
        return std::any{};
    });
    // CIP CyclopediaMapDataRaid / ImminentRaid (protocol subtypes when present).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapRaid", [this](GameObject*, const std::vector<std::any>& args) {
        const int x = static_cast<int>(argInt(args, 0));
        const int y = static_cast<int>(argInt(args, 1));
        const int z = static_cast<int>(argInt(args, 2));
        const worldmap::TWorldMapCoordinate coord(x, y, z);
        m_activeRaids.push_back(coord);
        Q_EMIT activeRaidAdded(coord);
        Q_EMIT activeRaidsChanged();
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapImminentRaid", [this](GameObject*, const std::vector<std::any>& args) {
        const int x = static_cast<int>(argInt(args, 0));
        const int y = static_cast<int>(argInt(args, 1));
        const int z = static_cast<int>(argInt(args, 2));
        TMapScaleFactor scale;
        if (const auto* value = args.size() > 3 ? std::any_cast<double>(&args[3]) : nullptr)
            scale.scale = *value;
        else if (args.size() > 3)
            scale.scale = static_cast<double>(argInt(args, 3));
        Q_EMIT imminentRaidAnnouncement(worldmap::TWorldMapCoordinate(x, y, z), scale);
        Q_EMIT changed();
        return std::any{};
    });
    // Host: a storage made after the player joined the session never sees
    // playerAdded; the position notify keeps it current.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPositionChange", [this](GameObject*, const std::vector<std::any>& args) {
        if (args.empty())
            return std::any{};
        if (const auto* position = std::any_cast<Position>(&args[0]))
            onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate(*position));
        return std::any{};
    });
    const auto bindPlayer = [this]() {
        if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance()) {
            connect(storage, &creatures::TCreatureStorage::playerAdded, this,
                    &TCyclopediaMapStorage::onPlayerCreatureAddedToGameSession,
                    static_cast<Qt::ConnectionType>(Qt::UniqueConnection));
        }
    };
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, bindPlayer);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameSessionChanged, this, bindPlayer);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_mainAreas.clear();
        m_discovered.clear();
        m_discoverable.clear();
        m_discoverySubarea = 0;
        m_poiTarget = 0;
        m_viewpoints.clear();
        m_passages.clear();
        m_subAreaViewpoints.clear();
        m_subAreaMonsters.clear();
        m_knownRaces.clear();
        m_donationGoal = 0;
        m_donations.clear();
        m_currentArea = 0;
        m_exploringSubarea = 0;
        m_playerPosition = {};
        m_playerPositionElement.area = worldmap::TWorldMapCuboid(m_playerPosition, worldmap::TWorldMapExtent(1, 1, 1));
        m_activeRaids.clear();
        Q_EMIT areaAndSubAreaDataChanged();
        Q_EMIT viewpointsChanged();
        Q_EMIT passagesChanged();
        Q_EMIT areaDonationsChanged();
        Q_EMIT playerAreaChanged();
        Q_EMIT playerPositionChanged();
        Q_EMIT activeRaidsChanged();
        Q_EMIT clickedAreaChanged();
        Q_EMIT changed();
    });
}

const TCyclopediaMapStorage::TMainArea* TCyclopediaMapStorage::mainArea(const int areaId) const
{
    const auto it = m_mainAreas.find(areaId);
    return it == m_mainAreas.end() ? nullptr : &it->second;
}

bool TCyclopediaMapStorage::isAreaCompleted(const int areaId) const
{
    const TMainArea* main = mainArea(areaId);
    return main && main->status == 3;
}

quint64 TCyclopediaMapStorage::donated(const int areaId) const
{
    const auto it = m_donations.find(areaId);
    return it == m_donations.end() ? 0 : it->second.second;
}

bool TCyclopediaMapStorage::isImprovedRespawnActive(const int areaId) const
{
    const auto it = m_donations.find(areaId);
    return it != m_donations.end() && it->second.first;
}

void TCyclopediaMapStorage::setSelectedId(const int id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    Q_EMIT clickedAreaChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::onPlayerCreatureAddedToGameSession(std::weak_ptr<creatures::TCreature> pPlayer)
{
    if (const std::shared_ptr<creatures::TCreature> player = pPlayer.lock())
        connect(player.get(), &creatures::TCreature::positionWasUpdated, this, &TCyclopediaMapStorage::onPlayerPositionWasUpdated);
    // The piece stays on the last known field until the player moves.
    m_playerPositionElement.area = worldmap::TWorldMapCuboid(m_playerPosition, worldmap::TWorldMapExtent(1, 1, 1));
    Q_EMIT playerPositionChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate WorldMapCoordinate)
{
    m_playerPosition = WorldMapCoordinate;
    m_playerPositionElement.area = worldmap::TWorldMapCuboid(m_playerPosition, worldmap::TWorldMapExtent(1, 1, 1));
    Q_EMIT playerPositionChanged();
    Q_EMIT changed();
}

QColor TCyclopediaMapStorage::subareaColor(const int subareaId) const
{
    if (m_selectedId == subareaId)
        return QColor(QStringLiteral("#ffd015"));
    if (isDiscovered(subareaId))
        return QColor(QStringLiteral("#66ff5e"));
    if (m_discoverySubarea == subareaId)
        return QColor(QStringLiteral("#cc3391"));
    if (isDiscoverable(subareaId))
        return QColor(QStringLiteral("#ffffff"));
    return QColor(QStringLiteral("#8f8f8f"));
}

QColor TCyclopediaMapStorage::areaColor(const int areaId) const
{
    if (m_selectedId == areaId)
        return QColor(QStringLiteral("#ffd015"));
    const TMainArea* main = mainArea(areaId);
    if (!main)
        return QColor(QStringLiteral("#ffffff"));
    if (main->status == 3)
        return QColor(QStringLiteral("#66ff5e"));
    if (main->progress > 0)
        return QColor(main->progress > 1.0f ? QStringLiteral("#8f8f8f") : QStringLiteral("#cc3391"));
    return QColor(QStringLiteral("#ffffff"));
}


TCyclopediaMapStorage::TSubAreaViewpointInfo TCyclopediaMapStorage::subAreaViewpointInfo(const quint16 subAreaId) const
{
    const auto it = m_subAreaViewpoints.find(subAreaId);
    return it == m_subAreaViewpoints.end() ? TSubAreaViewpointInfo{} : it->second;
}

void TCyclopediaMapStorage::setSubAreaViewpointInfo(const quint16 subAreaId, const quint8 count, const quint8 found)
{
    m_subAreaViewpoints[subAreaId] = { count, found };
}

std::vector<TCyclopediaMapStorage::TSubAreaMonster> TCyclopediaMapStorage::subAreaMonsters(const quint16 subAreaId) const
{
    const auto it = m_subAreaMonsters.find(subAreaId);
    return it == m_subAreaMonsters.end() ? std::vector<TSubAreaMonster>{} : it->second;
}

void TCyclopediaMapStorage::setSubAreaMonsters(const quint16 subAreaId, std::vector<TSubAreaMonster> monsters)
{
    m_subAreaMonsters[subAreaId] = std::move(monsters);
}

bool TCyclopediaMapStorage::isRaceKnown(const quint16 raceId) const
{
    const auto it = m_knownRaces.find(raceId);
    return it != m_knownRaces.end() && it->second;
}

void TCyclopediaMapStorage::setRaceKnown(const quint16 raceId, const bool known)
{
    m_knownRaces[raceId] = known;
}

}
