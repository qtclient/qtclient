#pragma once

#include "worldmap/creaturehudrenderprovider.h"

#include <cip_gen.h>

#include <QColor>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

class Outfit;

namespace tibia {
namespace config {
class TClientOptions;
}
namespace game {
class TPlayerData;
}
namespace worldmap {
class TWorldMapViewport;
}
}

namespace tibia {
namespace creatures {

class TCreatureStorage;

/// One battle list row: what TBattleListDataModel shows of a creature. The
/// render provider builds it through the setters, in its layout order.
struct TBattleListDrawCommand
{
    uint32_t creatureId = 0;
    QString name;
    float healthPercent = 0;
    /// The health bar's colour, from the health.
    QColor healthColor;
    float manaPercent = 0;
    /// The frames the creature shows on the map, and their colour names.
    std::vector<QColor> frames;
    QStringList frameColors;
    /// The icons of the creature's HUD, and their image URLs.
    worldmap::TCreatureHUDIconFlags status;
    QStringList statusIcons;
    // Host: CipSoft keeps the creature's outfit instance; qtclient its parts.
    uint outfitId = 0;
    bool outfitIsObject = false;
    QColor headColor;
    QColor torsoColor;
    QColor legsColor;
    QColor detailColor;
    bool firstAddOn = false;
    bool secondAddOn = false;
    int tutorialMarkerId = 0;
    /// The age order: the creature's unique id.
    uint visibleOrder = 0;
    uint distance = 0;
    bool temporarySelected = false;
    bool showMana = false;
    bool inSharedXpDistance = false;
    bool inViewport = false;
    int vocation = 0;
    bool fiendish = false;
    /// The server's status marks (0 a raid leader, 1 its minion).
    std::set<uint8_t> extraIcons;

    TBattleListDrawCommand& setCreatureId(uint32_t id);
    TBattleListDrawCommand& setVisibleOrder(uint order);
    TBattleListDrawCommand& setName(const QString& value);
    /// The health and the bar colour that goes with it.
    TBattleListDrawCommand& setHealthPercent(float percent);
    TBattleListDrawCommand& setManaPercent(float percent);
    TBattleListDrawCommand& setShowMana(bool show);
    TBattleListDrawCommand& setOutfit(const Outfit& outfit);
    TBattleListDrawCommand& setDistance(uint value);
    /// The frames and their colour names.
    TBattleListDrawCommand& setFrames(const std::vector<QColor>& colors);
    TBattleListDrawCommand& setTemporarySelected(bool selected);
    TBattleListDrawCommand& setTutorialMarkerId(int markerId);
    /// The HUD icons and their image URLs.
    TBattleListDrawCommand& setStatus(const worldmap::TCreatureHUDIconFlags& flags);
    TBattleListDrawCommand& setInSharedXpDistance(bool inDistance);
    TBattleListDrawCommand& setInViewport(bool visible);
    TBattleListDrawCommand& setFiendish(bool value);
    TBattleListDrawCommand& setVocation(int value);
    TBattleListDrawCommand& setExtraIcons(std::set<uint8_t> icons);

    bool leader() const { return extraIcons.contains(0); }
    bool leaderMinion() const { return extraIcons.contains(1); }

    /// CIP's draw command comparison.
    bool operator==(const TBattleListDrawCommand& other) const;

private:
    void updateFrameColors();
    void updateStatusIcons();
};

/// The battle list's rows (CIP: creatures::TBattleListRenderProvider, made by
/// the game session). It keeps the creatures' tutorial markers and which
/// creatures the last list showed.
class TBattleListRenderProvider final : public cip::tibia::creatures::TBattleListRenderProvider
{
    Q_OBJECT
public:
    TBattleListRenderProvider(std::shared_ptr<TCreatureStorage> creatureStorage,
                              std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport,
                              std::shared_ptr<game::TPlayerData> playerData, config::TClientOptions* clientOptions,
                              QObject* parent = nullptr);

    /// The rows of every creature the battle list can show, by creature id.
    /// A creature new to the list becomes the youngest; the first list after
    /// login keeps the creatures' ages.
    std::vector<TBattleListDrawCommand> drawCommands();
    void setTutorialMarker(uint32_t creatureId, int markerId);
    void removeAllTutorialMarkers();

private:
    std::shared_ptr<TCreatureStorage> m_creatureStorage;
    std::shared_ptr<worldmap::TWorldMapViewport> m_worldMapViewport;
    config::TClientOptions* m_clientOptions;
    std::shared_ptr<game::TPlayerData> m_playerData;
    std::map<uint32_t, int> m_tutorialMarkers;
    std::set<uint32_t> m_listedCreatureIds;
};

}
}
