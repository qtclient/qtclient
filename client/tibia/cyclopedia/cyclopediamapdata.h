#pragma once

#include "client/gamecore.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapareadata.h"

#include <cip_gen.h>

#include <QColor>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QString>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace cyclopedia {

/// CIP cyclopedia::TMapScaleFactor (moc imminent-raid / centerAt scale).
struct TMapScaleFactor
{
    double scale = 1.0;
};

struct TMapCoordinate
{
    int x = 0;
    int y = 0;
    int z = 0;
};

/// The player's Cyclopedia map progress (CIP: cyclopedia::TCyclopediaMapStorage).
/// The Cyclopedia protocol handler fills it from the map data message through
/// the setters below.
class TCyclopediaMapStorage final : public cip::tibia::cyclopedia::TCyclopediaMapStorage
{
    Q_OBJECT
public:
    struct TMainArea
    {
        quint16 id = 0;
        int status = 0;
        /// The wire byte / 100; above 1 = not explorable.
        float progress = 0;
    };

    struct TAreaDonation
    {
        quint16 id = 0;
        bool improvedRespawn = false;
        quint64 donated = 0;
    };

    struct TViewpoint
    {
        TMapCoordinate coordinate;
        int state = 0;
    };

    /// A passage and the races met beyond it.
    struct TPassage
    {
        TMapCoordinate coordinate;
        /// The subareas the passage leads to.
        std::vector<int> subAreaIds;
    };

    /// What the map knows of a subarea's viewpoints: how many there are to find
    /// and how many the player found. Every discovery area message leaves it,
    /// whether or not the player is discovering that subarea.
    struct TSubAreaViewpointInfo
    {
        quint8 poiTarget = 0;
        quint8 discoveredViewpoints = 0;
    };

    /// A monster of a subarea and its bestiary state (0, 1 or 2).
    struct TSubAreaMonster
    {
        quint16 raceId = 0;
        int state = 0;
    };

    explicit TCyclopediaMapStorage(QObject* parent = nullptr);
    ~TCyclopediaMapStorage() override;

    // The map data message's parts.
    /// The main areas with their status and progress and the two subarea lists.
    void setDiscoveryData(std::map<quint16, TMainArea> mainAreas, std::set<quint16> discoveredSubAreas,
                          std::set<quint16> discoverableSubAreas);
    void addActiveRaid(const worldmap::TWorldMapCoordinate& coordinate);
    void removeActiveRaid(const worldmap::TWorldMapCoordinate& coordinate);
    /// A raid announced for an area or a subarea; a rising state centres the map on it.
    void setImminentRaidOfArea(quint16 areaId, quint8 state);
    void setImminentRaidOfSubArea(quint16 subAreaId, quint8 state);
    /// A subarea's viewpoints (state 1 = found). An active message makes it the
    /// subarea being discovered and replaces the viewpoints; every message
    /// records the subarea's viewpoint info.
    void setDiscoveryArea(quint16 subAreaId, bool active, quint8 poiTarget,
                          const std::map<worldmap::TWorldMapCoordinate, int>& viewpoints);
    void setPassage(const worldmap::TWorldMapCoordinate& coordinate, const std::vector<quint16>& raceIds);
    void setMonsterKnown(quint16 raceId, bool known);
    void setSubAreaMonsters(quint16 subAreaId, std::vector<TSubAreaMonster> monsters);
    void setAreaDonations(std::map<quint16, TAreaDonation> donations);
    void setDonationGoal(quint64 donationGoal);
    void setExplorationRewardAmountGoStrength(int amount);
    void setPlayerArea(quint16 areaId);

    const TMainArea* mainArea(int areaId) const;
    /// The main area's progress; 0 for an unknown area.
    float mainAreaProgress(quint16 areaId) const;
    /// The main areas whose status is not 0.
    std::set<quint16> mainAreasWithStatus() const;
    bool isDiscovered(int subareaId) const { return m_discovered.contains(static_cast<quint16>(subareaId)); }
    bool isDiscoverable(int subareaId) const { return m_discoverable.contains(static_cast<quint16>(subareaId)); }
    /// The area's status is 3 (completed).
    bool isAreaCompleted(int areaId) const;
    /// The subarea of the last active discovery area message.
    int discoverySubarea() const { return m_discoverySubarea; }
    /// The subarea the player explores: the one being discovered.
    int exploringSubarea() const { return m_discoverySubarea; }
    /// The viewpoints to find in the subarea being discovered.
    int poiTarget() const { return m_poiTarget; }
    const std::vector<TViewpoint>& viewpoints() const { return m_viewpoints; }
    bool hasSubAreaViewpointInfo(quint16 subAreaId) const { return m_subAreaViewpointInfo.contains(subAreaId); }
    TSubAreaViewpointInfo subAreaViewpointInfo(quint16 subAreaId) const;
    const std::vector<TPassage>& passages() const { return m_passages; }
    /// The races of the passage on `coordinate`; empty for an unknown passage.
    std::vector<quint16> passageRaces(const worldmap::TWorldMapCoordinate& coordinate) const;
    bool isMonsterKnown(quint16 raceId) const;
    std::vector<TSubAreaMonster> subAreaMonsters(quint16 subAreaId) const;
    quint8 imminentRaidOfArea(quint16 areaId) const;
    quint8 imminentRaidOfSubArea(quint16 subAreaId) const;
    quint64 donationGoal() const { return m_donationGoal; }
    quint64 donated(int areaId) const;
    bool isImprovedRespawnActive(int areaId) const;
    /// The improved respawn of the area listing the subarea.
    bool isImprovedRespawnActiveInSubArea(quint16 subAreaId) const;
    /// The areas with donations, largest first, as a table; a short text without any.
    QString donationsTooltip() const;
    int currentArea() const { return m_playerArea; }
    /// The player marker (onPlayerPositionWasUpdated / onPositionChange).
    const worldmap::TWorldMapCoordinate& playerPosition() const { return m_playerPosition; }
    /// The player's field as a piece of map data (TPlayerPositionMapAreaData).
    const worldmap::TWorldMapAreaElement& playerPositionElement() const { return m_playerPositionElement; }
    const std::set<worldmap::TWorldMapCoordinate>& activeRaids() const { return m_activeRaids; }
    /// The passage, viewpoint and active raid pieces meeting `cuboid` on its floor.
    std::vector<worldmap::TWorldMapAreaElement> passagesIn(const worldmap::TWorldMapCuboid& cuboid) const;
    std::vector<worldmap::TWorldMapAreaElement> viewpointsIn(const worldmap::TWorldMapCuboid& cuboid) const;
    std::vector<worldmap::TWorldMapAreaElement> activeRaidsIn(const worldmap::TWorldMapCuboid& cuboid) const;
    /// The Go Strength the exploration of a subarea rewards.
    int explorationRewardAmountGoStrength() const { return m_explorationRewardAmountGoStrength; }
    QString areaName(quint16 areaId) const;
    QString subAreaName(quint16 subAreaId) const;
    /// The selected area or subarea → clickedAreaChanged.
    void setSelectedId(int id);
    int selectedId() const { return m_selectedId; }

    /// The caption colours.
    QColor subareaColor(int subareaId) const;
    QColor areaColor(int areaId) const;

public Q_SLOTS:
    /// The local player entered the session: follow its position.
    void onPlayerCreatureAddedToGameSession(std::weak_ptr<creatures::TCreature> pPlayer);
    /// The player's field changed (the player's positionWasUpdated).
    void onPlayerPositionWasUpdated(worldmap::TWorldMapCoordinate WorldMapCoordinate);

Q_SIGNALS:
    /// Tip aggregate for dialog/renderer; CIP-named signals fire alongside.
    void changed();
    /// CIP moc (skipped in cip_gen for TWorldMapCoordinate / TMapScaleFactor).
    void activeRaidAdded(worldmap::TWorldMapCoordinate WorldMapCoordinate);
    void imminentRaidAnnouncement(worldmap::TWorldMapCoordinate WorldMapCoordinate, TMapScaleFactor MapScaleFactor);

private:
    /// The active raid pieces again from the raids.
    void rebuildActiveRaids();
    void addViewpoint(const worldmap::TWorldMapCoordinate& coordinate, int state);
    /// The player's piece on the player's field.
    void refreshPlayerPosition();

    std::map<quint16, TMainArea> m_mainAreas;
    std::map<quint16, TAreaDonation> m_donations;
    std::set<quint16> m_discovered;
    std::set<quint16> m_discoverable;
    std::set<worldmap::TWorldMapCoordinate> m_activeRaids;
    std::map<quint16, quint8> m_imminentRaidAreas;
    std::map<quint16, quint8> m_imminentRaidSubAreas;
    quint16 m_discoverySubarea = 0;
    quint8 m_poiTarget = 0;
    std::vector<TViewpoint> m_viewpoints;
    std::map<quint16, TSubAreaViewpointInfo> m_subAreaViewpointInfo;
    std::map<quint16, std::vector<TSubAreaMonster>> m_subAreaMonsters;
    std::map<quint16, bool> m_knownMonsters;
    std::vector<TPassage> m_passages;
    int m_explorationRewardAmountGoStrength = 0;
    worldmap::TWorldMapAreaIndex m_passageElements;
    worldmap::TWorldMapAreaIndex m_viewpointElements;
    worldmap::TWorldMapAreaIndex m_activeRaidElements;
    worldmap::TWorldMapAreaElement m_playerPositionElement;
    worldmap::TWorldMapCoordinate m_playerPosition;
    int m_selectedId = 0;
    quint64 m_donationGoal = 0;
    quint16 m_playerArea = 0;
};

}
}
