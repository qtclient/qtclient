#pragma once

#include "creatures/creaturestorage.h"
#include "game/playerstates.h"
#include "game/playerstatestracker.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace dailyreward {
class TDailyRewardProtocolMessageHandler;
}
namespace game {
class TPlayerData;
}
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

class TWorldMapStorage;
class TWorldMapViewport;

/// The icons a creature's HUD shows around its name: the horizontal strip
/// (an NPC category, else party, skull and summon), the guild emblem and the
/// other icons below.
struct TCreatureHUDIconFlags
{
    /// 1 the player's own summon, 2 another player's.
    uint8_t summon = 0;
    uint8_t npcCategory = 0;
    uint8_t guild = 0;
    uint8_t party = 0;
    uint8_t skull = 0;
    std::vector<creatures::TCreatureIcon> icons;

    bool operator==(const TCreatureHUDIconFlags& other) const;
};

/// The creature's HUD icons followed by those of its status marks.
std::vector<creatures::TCreatureIcon> hudIconsWithStatusMarks(const creatures::TCreature& creature);

/// One creature's HUD for the frame being drawn, filled by
/// TCreatureHUDRenderProvider and copied into the creature's
/// TCreatureHUDController and TCreatureHUDQmlRenderInfo.
struct TCreatureHUDRenderInfo
{
    creatures::TCreatureID creatureId = 0;
    /// The HUD's anchor: the creature's top-left, walk, jump and elevation
    /// included, on the frame's sheared plane (map pixels, floor 0).
    TWorldMapSubfieldCoordinate anchor;
    bool showName = true;
    QString name;
    uint8_t healthPercent = 0;
    uint8_t manaPercent = 0;
    uint8_t manaShieldPercent = 0;
    bool showHealthBar = false;
    bool showManaBar = false;
    bool showManaShieldBar = false;
    bool isLocalPlayer = false;
    bool showBars = true;
    bool showArcs = false;
    bool showCombopointsAndSerene = false;
    bool showCombopointsAndSereneLeft = true;
    bool isSerene = false;
    float arcOpacity = 1.f;
    float arcDistance = 1.f;
    float arcHeight = 1.f;
    uint8_t comboPoints = 0;
    uint16_t arcWidth = 10;
    /// A floor between the first drawn one and the creature's covers it.
    bool covered = false;
    /// The light on the creature, 0 (dark) to 255.
    uint8_t lightness = 0xff;
    float scaleFactor = 1.f;
    QList<game::PlayerStateDisplayInfo> playerStates;
    bool showSpecialConditions = false;
    TCreatureHUDIconFlags iconFlags;
    /// The status marks the server sets on the creature (0 a raid leader, 1 its minion).
    std::set<uint8_t> statusMarks;
};

/// Builds the HUD records of the creatures in the viewport's visible area
/// (CIP: TCreatureHUDRenderProvider, a game-session object). The display
/// options are cached and re-read when the client options announce a change.
class TCreatureHUDRenderProvider final : public cip::tibia::worldmap::TCreatureHUDRenderProvider
{
    Q_OBJECT
public:
    TCreatureHUDRenderProvider(std::shared_ptr<TWorldMapStorage> worldMapStorage,
                               std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                               std::shared_ptr<TWorldMapViewport> worldMapViewport,
                               std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider,
                               std::shared_ptr<game::TPlayerData> playerData, config::TClientOptions* clientOptions,
                               std::shared_ptr<dailyreward::TDailyRewardProtocolMessageHandler> dailyRewardProtocolMessageHandler = nullptr,
                               QObject* parent = nullptr);

    /// The records of the creatures in the visible area that show a HUD,
    /// the player's always considered.
    virtual std::vector<TCreatureHUDRenderInfo> creatureHUDRenderInfos() const;
    /// `creature`'s record; `firstVisibleFloor` is the first floor drawn.
    virtual TCreatureHUDRenderInfo creatureHUDRenderInfo(const creatures::TCreature& creature, int firstVisibleFloor) const;
    /// No floor from `position`'s down to `firstVisibleFloor` holds a bank over
    /// its field in `worldMapStorage` (true without a storage).
    static bool isUncovered(const std::shared_ptr<TWorldMapStorage>& worldMapStorage, const TWorldMapCoordinate& position,
                            int firstVisibleFloor);

public Q_SLOTS:
    /// The daily reward handler said whether the player is in a resting area.
    void setInRestingArea(bool inRestingArea) { m_inRestingArea = inRestingArea; }

private:
    void readOptions();
    /// Appends the player's creature when none of `creatures` is the player.
    void appendPlayer(std::vector<std::shared_ptr<creatures::TCreature>>& creatures) const;
    /// Moves the anchor of the outfits that stand taller than their field.
    static TWorldMapSubfieldCoordinate adjustedAnchor(const TWorldMapSubfieldCoordinate& anchor,
                                                     const creatures::TCreature& creature);
    /// The frame's HUD anchor of `creatureId`; false when the frame does not
    /// draw the creature.
    bool frameAnchor(creatures::TCreatureID creatureId, TWorldMapSubfieldCoordinate& anchor) const;
    /// The light on `anchor`'s field centre, 0 to 255.
    uint8_t lightnessAt(const TWorldMapSubfieldCoordinate& anchor) const;

    std::shared_ptr<TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<TWorldMapViewport> m_worldMapViewport;
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::shared_ptr<game::TPlayerData> m_playerData;
    config::TClientOptions* m_clientOptions = nullptr;
    std::shared_ptr<dailyreward::TDailyRewardProtocolMessageHandler> m_dailyRewardProtocolMessageHandler;
    /// The player is in a resting area. Host: the player's states are built
    /// by m_playerStates, which follows the resting area itself.
    bool m_inRestingArea = false;
    /// The player's special conditions with the HUD filter (resting area and
    /// options included).
    mutable game::TPlayerStatesTracker m_playerStates;

    bool m_playerHudEnabled = true;
    bool m_playerShowName = true;
    bool m_playerHudShowBars = true;
    bool m_playerHudShowArcs = true;
    bool m_playerHudShowHarmonyLeft = true;
    bool m_playerShowHealth = true;
    bool m_playerShowMana = true;
    bool m_playerShowHarmonySerene = true;
    bool m_playerShowMarks = true;
    bool m_creatureHudEnabled = true;
    bool m_creatureShowName = true;
    bool m_creatureShowHealth = true;
    bool m_creatureShowMarks = true;
    bool m_creatureShowNpcIcon = true;
    float m_playerHudArcOpacity = 1.f;
    float m_playerHudArcDistance = 1.f;
    float m_playerHudArcHeight = 1.f;
    int m_playerHudArcWidth = 10;
};

}
}
