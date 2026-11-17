#pragma once

#include "client/gamecore.h"
#include "worldmap/coordinates.h"

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
        std::vector<int> raceIds;
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
    quint64 donationGoal() const { return m_donationGoal; }
    quint64 donated(int areaId) const;
    /// CIP .
    bool isImprovedRespawnActive(int areaId) const;
    int currentArea() const { return m_currentArea; }
    int exploringSubarea() const { return m_exploringSubarea; }
    /// CIP player marker (onPlayerPositionWasUpdated / onPositionChange).
    const worldmap::TWorldMapCoordinate& playerPosition() const { return m_playerPosition; }
    const std::vector<worldmap::TWorldMapCoordinate>& activeRaids() const { return m_activeRaids; }
    /// CIP DAT Go Strength shown in the map info tooltip (dialog default 25).
    int explorationRewardAmountGoStrength() const { return m_explorationRewardAmountGoStrength; }
    /// CIP (+0x300): the selected area or subarea → clickedAreaChanged.
    void setSelectedId(int id);

    /// CIP / : the caption colours.
    QColor subareaColor(int subareaId) const;
    QColor areaColor(int areaId) const;

public Q_SLOTS:
    /// CIP moc slot — local player entered the session.
    void onPlayerCreatureAddedToGameSession(std::weak_ptr<creatures::TCreature> pPlayer);
    /// CIP moc slot — player field update (also fed by onPositionChange).
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
    quint64 m_donationGoal = 0;
    std::map<int, std::pair<bool, quint64>> m_donations;
    int m_currentArea = 0;
    int m_exploringSubarea = 0;
    int m_selectedId = 0;
    worldmap::TWorldMapCoordinate m_playerPosition;
    std::vector<worldmap::TWorldMapCoordinate> m_activeRaids;
    int m_explorationRewardAmountGoStrength = 25;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
