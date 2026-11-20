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

/// The player's Cyclopedia map progress from the 0xDD packets (CIP:
/// TCyclopediaMapStorage / cip_gen moc, fed by handleCyclopediaMapMessage
/// → qtclient onCyclopediaMap* notifies).
class TCyclopediaMapStorage final : public cip::tibia::cyclopedia::TCyclopediaMapStorage
{
    Q_OBJECT
public:
    struct TMainArea
    {
        int status = 0;
        /// CIP : the wire byte / 100; above 1 = not explorable.
        float progress = 0;
    };

    struct TViewpoint
    {
        TMapCoordinate coordinate;
        int state = 0;
    };

    /// CIP TPassageMapAreaData / CyclopediaMapDataPassage (0xDD subtype 6).
    struct TPassage
    {
        TMapCoordinate coordinate;
        /// The subareas the passage leads to.
        std::vector<int> subAreaIds;
    };

    /// A subarea's viewpoints as the last SetDiscoveryArea for it told: how many
    /// it has and how many are found.
    struct TSubAreaViewpointInfo
    {
        quint8 count = 0;
        quint8 found = 0;
    };

    /// A creature of a subarea (SubAreaMonsters): its race and how often it
    /// occurs there (0 common, 1 rare, 2 dynamic).
    struct TSubAreaMonster
    {
        quint16 raceId = 0;
        int occurrence = 0;
    };

    explicit TCyclopediaMapStorage(QObject* parent = nullptr);

    const TMainArea* mainArea(int areaId) const;
    /// CIP / : the two subarea lists of DiscoveryData.
    bool isDiscovered(int subareaId) const { return m_discovered.contains(subareaId); }
    bool isDiscoverable(int subareaId) const { return m_discoverable.contains(subareaId); }
    /// CIP : the area's status is 3 (completed).
    bool isAreaCompleted(int areaId) const;
    /// CIP +0xc8: the subarea of the last active SetDiscoveryArea.
    int discoverySubarea() const { return m_discoverySubarea; }
    /// CIP SetDiscoveryArea poiTarget (session total; usually matches viewpoints.size).
    int poiTarget() const { return m_poiTarget; }
    const std::vector<TViewpoint>& viewpoints() const { return m_viewpoints; }
    const std::vector<TPassage>& passages() const { return m_passages; }
    /// A SetDiscoveryArea for the subarea arrived, active or not.
    bool hasSubAreaViewpointInfo(quint16 subAreaId) const { return m_subAreaViewpoints.contains(subAreaId); }
    /// The subarea's last viewpoint info; none (0 of 0) before one arrived.
    TSubAreaViewpointInfo subAreaViewpointInfo(quint16 subAreaId) const;
    void setSubAreaViewpointInfo(quint16 subAreaId, quint8 count, quint8 found);
    /// The creatures of the subarea; empty when none were sent.
    std::vector<TSubAreaMonster> subAreaMonsters(quint16 subAreaId) const;
    void setSubAreaMonsters(quint16 subAreaId, std::vector<TSubAreaMonster> monsters);
    /// The player knows the race from the bestiary; false for a race not sent.
    bool isRaceKnown(quint16 raceId) const;
    void setRaceKnown(quint16 raceId, bool known);
    quint64 donationGoal() const { return m_donationGoal; }
    quint64 donated(int areaId) const;
    /// CIP .
    bool isImprovedRespawnActive(int areaId) const;
    int currentArea() const { return m_currentArea; }
    int exploringSubarea() const { return m_exploringSubarea; }
    /// CIP player marker (onPlayerPositionWasUpdated / onPositionChange).
    const worldmap::TWorldMapCoordinate& playerPosition() const { return m_playerPosition; }
    /// The player's field as a piece of map data (TPlayerPositionMapAreaData).
    const worldmap::TWorldMapAreaElement& playerPositionElement() const { return m_playerPositionElement; }
    const std::vector<worldmap::TWorldMapCoordinate>& activeRaids() const { return m_activeRaids; }
    /// CIP DAT Go Strength shown in the map info tooltip (dialog default 25).
    int explorationRewardAmountGoStrength() const { return m_explorationRewardAmountGoStrength; }
    /// CIP (+0x300): the selected area or subarea → clickedAreaChanged.
    void setSelectedId(int id);

    /// CIP / : the caption colours.
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
    std::map<int, TMainArea> m_mainAreas;
    std::set<int> m_discovered;
    std::set<int> m_discoverable;
    int m_discoverySubarea = 0;
    int m_poiTarget = 0;
    std::vector<TViewpoint> m_viewpoints;
    std::vector<TPassage> m_passages;
    std::map<quint16, TSubAreaViewpointInfo> m_subAreaViewpoints;
    std::map<quint16, std::vector<TSubAreaMonster>> m_subAreaMonsters;
    std::map<quint16, bool> m_knownRaces;
    quint64 m_donationGoal = 0;
    std::map<int, std::pair<bool, quint64>> m_donations;
    int m_currentArea = 0;
    int m_exploringSubarea = 0;
    int m_selectedId = 0;
    worldmap::TWorldMapAreaElement m_playerPositionElement;
    worldmap::TWorldMapCoordinate m_playerPosition;
    std::vector<worldmap::TWorldMapCoordinate> m_activeRaids;
    int m_explorationRewardAmountGoStrength = 25;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
