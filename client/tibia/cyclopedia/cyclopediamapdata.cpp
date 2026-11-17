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
        if (!argInt(args, 1))
            return std::any{};
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
        const int raceCount = (*row)[3];
        for (int i = 0; i < raceCount && 4 + i < static_cast<int>(row->size()); ++i)
            passage.raceIds.push_back((*row)[4 + i]);
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
        m_donationGoal = 0;
        m_donations.clear();
        m_currentArea = 0;
        m_exploringSubarea = 0;
        m_playerPosition = {};
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
        onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate(player->position()));
}

void TCyclopediaMapStorage::onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate WorldMapCoordinate)
{
    if (m_playerPosition == WorldMapCoordinate)
        return;
    m_playerPosition = WorldMapCoordinate;
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

}
