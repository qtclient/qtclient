#include "cyclopediamapdata.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/gamesessionbase.h"
#include <algorithm>

#include "client/gameclient.h"
#include "client/gamecore.h"
#include "creatures/creaturestorage.h"

#include <QCoreApplication>
#include <QLocale>

#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::cyclopedia {

namespace {

TCyclopediaMapStorage* g_liveMapStorage = nullptr;

TMapCoordinate mapCoordinate(const worldmap::TWorldMapCoordinate& coordinate)
{
    return { coordinate.x, coordinate.y, coordinate.z };
}

bool isAt(const TMapCoordinate& coordinate, const worldmap::TWorldMapCoordinate& field)
{
    return coordinate.x == field.x && coordinate.y == field.y && coordinate.z == field.z;
}

worldmap::TWorldMapCuboid fieldCuboid(const worldmap::TWorldMapCoordinate& coordinate)
{
    return worldmap::TWorldMapCuboid(coordinate, worldmap::TWorldMapExtent(1, 1, 1));
}

}

TCyclopediaMapStorage::TCyclopediaMapStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCyclopediaMapStorage(parent)
    , m_playerPositionElement{ worldmap::TWorldMapCuboid(worldmap::TWorldMapCoordinate(0, 0, 0), worldmap::TWorldMapExtent(0, 0, 1)),
                               worldmap::TPlayerPositionMapAreaData{} }
{
    g_liveMapStorage = this;
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
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, &TCyclopediaMapStorage::reset);
}

TCyclopediaMapStorage::~TCyclopediaMapStorage()
{
    if (g_liveMapStorage == this)
        g_liveMapStorage = nullptr;
}

TCyclopediaMapStorage* TCyclopediaMapStorage::liveInstance()
{
    return g_liveMapStorage;
}

void TCyclopediaMapStorage::reset()
{
    // Host: CipSoft makes the storage per game session; this one outlives it.
    m_mainAreas.clear();
    m_donations.clear();
    m_discovered.clear();
    m_discoverable.clear();
    m_activeRaids.clear();
    m_imminentRaidAreas.clear();
    m_imminentRaidSubAreas.clear();
    m_discoverySubarea = 0;
    m_poiTarget = 0;
    m_viewpoints.clear();
    m_subAreaViewpointInfo.clear();
    m_subAreaMonsters.clear();
    m_knownMonsters.clear();
    m_passages.clear();
    m_explorationRewardAmountGoStrength = 0;
    m_passageElements.clear();
    m_viewpointElements.clear();
    m_activeRaidElements.clear();
    m_playerPosition = {};
    m_playerPositionElement.area = fieldCuboid(m_playerPosition);
    m_donationGoal = 0;
    m_playerArea = 0;
    Q_EMIT areaAndSubAreaDataChanged();
    Q_EMIT viewpointsChanged();
    Q_EMIT passagesChanged();
    Q_EMIT areaDonationsChanged();
    Q_EMIT playerAreaChanged();
    Q_EMIT playerPositionChanged();
    Q_EMIT activeRaidsChanged();
    Q_EMIT clickedAreaChanged();
    Q_EMIT explorationRewardAmountGoStrengthChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setDiscoveryData(std::map<quint16, TMainArea> mainAreas, std::set<quint16> discoveredSubAreas,
                                             std::set<quint16> discoverableSubAreas)
{
    m_mainAreas = std::move(mainAreas);
    m_discovered = std::move(discoveredSubAreas);
    m_discoverable = std::move(discoverableSubAreas);
    Q_EMIT areaAndSubAreaDataChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::addActiveRaid(const worldmap::TWorldMapCoordinate& coordinate)
{
    m_activeRaids.insert(coordinate);
    rebuildActiveRaids();
    Q_EMIT activeRaidAdded(coordinate);
    Q_EMIT changed();
}

void TCyclopediaMapStorage::removeActiveRaid(const worldmap::TWorldMapCoordinate& coordinate)
{
    if (m_activeRaids.erase(coordinate) == 0)
        return;
    rebuildActiveRaids();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::rebuildActiveRaids()
{
    // Each piece announces itself; removing the last raid announces nothing.
    m_activeRaidElements.clear();
    for (const worldmap::TWorldMapCoordinate& raid : m_activeRaids) {
        m_activeRaidElements.insert(fieldCuboid(raid), worldmap::TActiveRaidMapAreaData{});
        Q_EMIT activeRaidsChanged();
    }
}

void TCyclopediaMapStorage::setImminentRaidOfArea(const quint16 areaId, const quint8 state)
{
    const auto [it, added] = m_imminentRaidAreas.try_emplace(areaId, 0);
    const quint8 previous = it->second;
    if (!added && previous == state)
        return;
    it->second = state;
    if (added ? state == 0 : state <= previous)
        return;
    const auto* area = TCyclopediaMapStaticDataStorage::instance().area(areaId);
    const worldmap::TWorldMapCoordinate coordinate = area ? area->labelCoordinate : worldmap::TWorldMapCoordinate(0, 0, 0);
    Q_EMIT imminentRaidAnnouncement(coordinate, TMapScaleFactor{ 1.0 / 64.0 });
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setImminentRaidOfSubArea(const quint16 subAreaId, const quint8 state)
{
    const auto [it, added] = m_imminentRaidSubAreas.try_emplace(subAreaId, 0);
    const quint8 previous = it->second;
    if (!added && previous == state)
        return;
    it->second = state;
    if (added ? state == 0 : state <= previous)
        return;
    const auto* subArea = TCyclopediaMapStaticDataStorage::instance().subArea(subAreaId);
    const worldmap::TWorldMapCoordinate coordinate = subArea ? subArea->labelCoordinate : worldmap::TWorldMapCoordinate(0, 0, 0);
    Q_EMIT imminentRaidAnnouncement(coordinate, TMapScaleFactor{ 1.0 / 16.0 });
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setDiscoveryArea(const quint16 subAreaId, const bool active, const quint8 poiTarget,
                                             const std::map<worldmap::TWorldMapCoordinate, int>& viewpoints)
{
    if (active) {
        m_discoverySubarea = subAreaId;
        m_poiTarget = poiTarget;
        m_viewpoints.clear();
        m_viewpointElements.clear();
        for (const auto& [coordinate, state] : viewpoints)
            addViewpoint(coordinate, state);
    }
    quint8 discovered = 0;
    for (const auto& [coordinate, state] : viewpoints) {
        if (state == 1)
            ++discovered;
    }
    m_subAreaViewpointInfo.insert_or_assign(subAreaId, TSubAreaViewpointInfo{ poiTarget, discovered });
    Q_EMIT viewpointsChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::addViewpoint(const worldmap::TWorldMapCoordinate& coordinate, const int state)
{
    const auto it = std::ranges::find_if(m_viewpoints, [&](const TViewpoint& viewpoint) { return isAt(viewpoint.coordinate, coordinate); });
    if (it != m_viewpoints.end())
        it->state = state;
    else
        m_viewpoints.push_back({ mapCoordinate(coordinate), state });
    m_viewpointElements.insert(fieldCuboid(coordinate), worldmap::TViewpointMapAreaData{ state == 1 });
    Q_EMIT viewpointsChanged();
}

void TCyclopediaMapStorage::setPassage(const worldmap::TWorldMapCoordinate& coordinate, const std::vector<quint16>& raceIds)
{
    TPassage passage;
    passage.coordinate = mapCoordinate(coordinate);
    passage.subAreaIds.assign(raceIds.begin(), raceIds.end());
    const auto it = std::ranges::find_if(m_passages, [&](const TPassage& known) { return isAt(known.coordinate, coordinate); });
    if (it != m_passages.end())
        *it = std::move(passage);
    else
        m_passages.push_back(std::move(passage));
    m_passageElements.insert(fieldCuboid(coordinate), worldmap::TPassageMapAreaData{ raceIds, coordinate });
    Q_EMIT passagesChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setMonsterKnown(const quint16 raceId, const bool known)
{
    m_knownMonsters.insert_or_assign(raceId, known);
}

void TCyclopediaMapStorage::setSubAreaMonsters(const quint16 subAreaId, std::vector<TSubAreaMonster> monsters)
{
    m_subAreaMonsters[subAreaId] = std::move(monsters);
}

void TCyclopediaMapStorage::setAreaDonations(std::map<quint16, TAreaDonation> donations)
{
    m_donations = std::move(donations);
    Q_EMIT areaDonationsChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setDonationGoal(const quint64 donationGoal)
{
    m_donationGoal = donationGoal;
    Q_EMIT areaDonationsChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setExplorationRewardAmountGoStrength(const int amount)
{
    if (m_explorationRewardAmountGoStrength == amount)
        return;
    m_explorationRewardAmountGoStrength = amount;
    Q_EMIT explorationRewardAmountGoStrengthChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::setPlayerArea(const quint16 areaId)
{
    m_playerArea = areaId;
    Q_EMIT playerAreaChanged();
    Q_EMIT changed();
}

const TCyclopediaMapStorage::TMainArea* TCyclopediaMapStorage::mainArea(const int areaId) const
{
    const auto it = m_mainAreas.find(static_cast<quint16>(areaId));
    return it == m_mainAreas.end() ? nullptr : &it->second;
}

float TCyclopediaMapStorage::mainAreaProgress(const quint16 areaId) const
{
    const TMainArea* main = mainArea(areaId);
    return main ? main->progress : 0.0f;
}

std::set<quint16> TCyclopediaMapStorage::mainAreasWithStatus() const
{
    std::set<quint16> areas;
    for (const auto& [areaId, area] : m_mainAreas) {
        if (area.status != 0)
            areas.insert(area.id);
    }
    return areas;
}

bool TCyclopediaMapStorage::isAreaCompleted(const int areaId) const
{
    const TMainArea* main = mainArea(areaId);
    return main && main->status == 3;
}

TCyclopediaMapStorage::TSubAreaViewpointInfo TCyclopediaMapStorage::subAreaViewpointInfo(const quint16 subAreaId) const
{
    const auto it = m_subAreaViewpointInfo.find(subAreaId);
    return it == m_subAreaViewpointInfo.end() ? TSubAreaViewpointInfo{} : it->second;
}

std::vector<quint16> TCyclopediaMapStorage::passageRaces(const worldmap::TWorldMapCoordinate& coordinate) const
{
    std::vector<quint16> races;
    const auto it = std::ranges::find_if(m_passages, [&](const TPassage& known) { return isAt(known.coordinate, coordinate); });
    if (it != m_passages.end()) {
        for (const int race : it->subAreaIds)
            races.push_back(static_cast<quint16>(race));
    }
    return races;
}

bool TCyclopediaMapStorage::isMonsterKnown(const quint16 raceId) const
{
    const auto it = m_knownMonsters.find(raceId);
    return it != m_knownMonsters.end() && it->second;
}

std::vector<TCyclopediaMapStorage::TSubAreaMonster> TCyclopediaMapStorage::subAreaMonsters(const quint16 subAreaId) const
{
    const auto it = m_subAreaMonsters.find(subAreaId);
    return it == m_subAreaMonsters.end() ? std::vector<TSubAreaMonster>() : it->second;
}

quint8 TCyclopediaMapStorage::imminentRaidOfArea(const quint16 areaId) const
{
    const auto it = m_imminentRaidAreas.find(areaId);
    return it == m_imminentRaidAreas.end() ? 0 : it->second;
}

quint8 TCyclopediaMapStorage::imminentRaidOfSubArea(const quint16 subAreaId) const
{
    const auto it = m_imminentRaidSubAreas.find(subAreaId);
    return it == m_imminentRaidSubAreas.end() ? 0 : it->second;
}

quint64 TCyclopediaMapStorage::donated(const int areaId) const
{
    const auto it = m_donations.find(static_cast<quint16>(areaId));
    return it == m_donations.end() ? 0 : it->second.donated;
}

bool TCyclopediaMapStorage::isImprovedRespawnActive(const int areaId) const
{
    const auto it = m_donations.find(static_cast<quint16>(areaId));
    return it != m_donations.end() && it->second.improvedRespawn;
}

bool TCyclopediaMapStorage::isImprovedRespawnActiveInSubArea(const quint16 subAreaId) const
{
    return isImprovedRespawnActive(TCyclopediaMapStaticDataStorage::instance().areaIdOfSubArea(subAreaId));
}

QString TCyclopediaMapStorage::donationsTooltip() const
{
    std::vector<std::pair<quint16, quint64>> rows;
    for (const auto& [areaId, donation] : m_donations) {
        if (donation.donated != 0)
            rows.emplace_back(areaId, donation.donated);
    }
    if (rows.empty())
        return qtTrId("area_value_tooltip_no_values");
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    const QLocale english(QLocale::English);
    QString html = QStringLiteral("<table><tr><th colspan=2 align=center>");
    html += qtTrId("area_value_tooltip");
    html += QStringLiteral("</th></tr>");
    for (const auto& [areaId, donated] : rows) {
        html += QStringLiteral("<tr><td>");
        html += areaName(areaId);
        html += QStringLiteral("</td><td align=right>");
        html += english.toString(static_cast<qulonglong>(donated));
        html += QStringLiteral("</td></tr>");
    }
    html += QStringLiteral("</table>");
    return html;
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStorage::passagesIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_passageElements.query(cuboid, 1);
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStorage::viewpointsIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_viewpointElements.query(cuboid, 1);
}

std::vector<worldmap::TWorldMapAreaElement> TCyclopediaMapStorage::activeRaidsIn(const worldmap::TWorldMapCuboid& cuboid) const
{
    return m_activeRaidElements.query(cuboid, 1);
}

QString TCyclopediaMapStorage::areaName(const quint16 areaId) const
{
    return TCyclopediaMapStaticDataStorage::instance().areaName(areaId);
}

QString TCyclopediaMapStorage::subAreaName(const quint16 subAreaId) const
{
    return TCyclopediaMapStaticDataStorage::instance().subAreaName(subAreaId);
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
    m_playerPositionElement.area = fieldCuboid(m_playerPosition);
    Q_EMIT playerPositionChanged();
    Q_EMIT changed();
}

void TCyclopediaMapStorage::onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate WorldMapCoordinate)
{
    m_playerPosition = WorldMapCoordinate;
    refreshPlayerPosition();
}

void TCyclopediaMapStorage::refreshPlayerPosition()
{
    m_playerPositionElement.area = fieldCuboid(m_playerPosition);
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
