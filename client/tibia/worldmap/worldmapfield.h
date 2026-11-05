#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"
#include "client/declarations.h"
#include "client/staticdata.h"
#include "client/statictext.h"

#include <memory>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
}

namespace tibia {
namespace worldmap {

/// Stack row that references a live creature by id (outfit appearance on the field stack).
struct TStackCreatureEntry
{
    int stackIndex = -1;
    uint32_t creatureId = 0;
    appearances::TAppearanceInstancePtr appearance;
};

/// One aware-range field (CIP: TWorldMapField). Owns the appearance stack (+ item/creature refs) for qtclient play.
class TWorldMapField final : public cip::tibia::worldmap::TWorldMapField
{
    Q_OBJECT
public:
    explicit TWorldMapField(QObject* parent = nullptr);

    void setCoordinate(const TWorldMapCoordinate& coordinate)
    {
        m_coordinate = coordinate;
        syncPositionCache();
    }
    const TWorldMapCoordinate& coordinate() const { return m_coordinate; }

    Position getPosition() const { return m_coordinate.toPosition(); }
    const Position& position() const { return m_positionCache; }
    void syncPositionCache() { m_positionCache = getPosition(); }

    void onAddInMapView();

    void clean();

    void addWalkingCreature(uint32_t creatureId);
    void removeWalkingCreature(uint32_t creatureId);

    void addThing(const ThingPtr& thing, int stackPos);
    /// Protocol magic effect: CIP field appearance-only (no Effect Thing).
    void addEffectAppearance(const appearances::TAppearanceInstancePtr& appearance);
    /// Protocol item place: appearance-only stack entry (no Item::create / ItemPtr).
    void addAppearance(const appearances::TAppearanceInstancePtr& appearance, int stackPos);
    /// Phase 5.0-C: place creature from storage id + outfit appearance (no CreaturePtr).
    void addCreatureId(uint32_t creatureId, int stackPos);
    bool removeThing(ThingPtr thing);
    /// Remove stack entry by creature id without requiring Map `Creature`.
    bool removeCreatureById(uint32_t creatureId);
    ThingPtr getThing(int stackPos) const;
    bool hasThing(const ThingPtr& thing);
    int getThingStackPos(const ThingPtr& thing);
    int creatureStackPos(uint32_t creatureId) const;

    /// LocalPlayer-only CreaturePtr stub; play UI uses stackCreatureId / getStoredCreatures.
    CreaturePtr getTopCreature(bool checkAround = false);

    int getDrawElevation() const { return m_drawElevation; }
    const std::vector<uint32_t>& walkingCreatureIds() const { return m_walkingCreatureIds; }
    /// CipSoft field stack (items/effects + creature outfit appearances; gameplay items/creatures resolved on demand).
    const std::vector<appearances::TAppearanceInstancePtr>& appearances() const { return m_appearanceCache; }
    appearances::TAppearanceInstancePtr appearanceAt(int stackPos) const;
    uint16_t itemClientIdAt(int stackPos) const;
    uint32_t stackCreatureId(int stackPos) const;
    bool removeThingAtStackPos(int stackPos);
    bool removeEffectByClientId(uint16_t clientId);
    bool removeEffectAppearance(const appearances::TAppearanceInstancePtr& appearance);
    /// Drop finished field effects (CIP animator active flag / duration).
    void purgeExpiredEffects();
    /// Refresh outfit/direction appearance entries when the live creature changes look.
    void syncCreatureStackAppearance(uint32_t creatureId);
    const std::vector<appearances::TAppearanceInstancePtr>& effectAppearances() const { return m_effectAppearances; }
    /// LocalPlayer-only CreaturePtr list; Map spectators + tests still call this.
    std::vector<CreaturePtr> getCreatures() const;
    /// CipSoft path: creature ids on this field resolved through `TCreatureStorage`.
    std::vector<std::shared_ptr<tibia::creatures::TCreature>> getStoredCreatures() const;
    std::vector<TStackCreatureEntry> stackCreatureEntries() const;

    /// Non-creature stack rows as CIP appearance/typeId hits (no ItemPtr keep-alive).
    struct TFieldItemHit
    {
        uint16_t typeId = 0;
        int stackPos = -1;
        bool ok() const { return typeId != 0 && stackPos >= 0; }
    };
    std::vector<TFieldItemHit> itemHits() const;
    /// Top use target as typeId + stackPos (CIP use / rope / ladder; no ThingPtr).
    TFieldItemHit topUseHit() const;
    int getGroundSpeed();
    uint8_t getMinimapColorByte();
    int getThingCount() { return static_cast<int>(m_stack.size()); }
    int thingCount() const { return static_cast<int>(m_stack.size()); }

    bool isWalkable(bool ignoreCreatures = false) const;
    bool isTravelSoftBlocker();
    bool isClickable();
    bool isPathable() { return (m_thingTypeFlag & NOT_PATHABLE) == 0; }
    bool isFullGround() const { return m_thingTypeFlag & FULL_GROUND; }
    bool isFullyOpaque() const;
    bool isSingleDimension() { return (m_thingTypeFlag & NOT_SINGLE_DIMENSION) == 0 && m_walkingCreatureIds.empty(); }
    bool isLookPossible() { return (m_thingTypeFlag & BLOCK_PROJECTTILE) == 0; }
    bool isEmpty() const { return m_stack.empty(); }
    bool isDrawable() { return !isEmpty() || !m_walkingCreatureIds.empty() || hasEffect(); }
    bool isCovered(int8_t firstFloor);
    bool isCompletelyCovered(uint8_t firstFloor, bool resetCache = false);
    void invalidateCoverCache() { m_isCompletelyCovered = m_isCovered = 0; }
    bool isLoading() const;

    bool hasBlockingCreature() const;
    bool hasBlockingBottomItem() const;

    bool hasEffect() const { return !m_effectAppearances.empty(); }
    bool hasGround() const;
    bool hasTopGround(const bool ignoreBorder = false) const;

    bool hasCreatures() const { return (m_thingTypeFlag & HAS_CREATURE) != 0; }
    bool hasWalkingCreature() { return !m_walkingCreatureIds.empty(); }

    /// LocalPlayer-only CreaturePtr spectators; Map still collects via this path.
    void appendSpectators(std::vector<CreaturePtr>& out) const;

    bool hasTopItem() const { return m_thingTypeFlag & HAS_TOP_ITEM; }
    bool hasCommonItem() const { return m_thingTypeFlag & HAS_COMMON_ITEM; }
    bool hasBottomItem() const { return m_thingTypeFlag & HAS_BOTTOM_ITEM; }

    bool hasIgnoreLook() { return m_thingTypeFlag & IGNORE_LOOK; }
    bool hasDisplacement() const { return m_thingTypeFlag & HAS_DISPLACEMENT; }
    bool hasLight() const { return m_thingTypeFlag & HAS_LIGHT; }
    bool hasTallThings() const { return m_thingTypeFlag & HAS_TALL_THINGS; }
    bool hasWideThings() const { return m_thingTypeFlag & HAS_WIDE_THINGS; }
    bool hasTallThings2() const { return m_thingTypeFlag & HAS_TALL_THINGS_2; }
    bool hasWideThings2() const { return m_thingTypeFlag & HAS_WIDE_THINGS_2; }
    bool hasWall() const { return m_thingTypeFlag & HAS_WALL; }

    bool mustHookSouth() const { return m_thingTypeFlag & HAS_HOOK_SOUTH; }
    bool mustHookEast() const { return m_thingTypeFlag & HAS_HOOK_EAST; }

    bool limitsFloorsView(bool isFreeView = false);

    bool canShade() const { return isFullyOpaque() || hasGround() || hasTopGround() || isFullGround(); }
    bool canRender(uint32_t& flags, const Position& cameraPosition, AwareRange viewPort);
    bool canErase()
    {
        return !isDrawable() && m_minimapColor == 0
            ;
    }

    bool hasElevation(const int elevation = 1) { return m_elevation >= elevation; }
    bool hasFloorChange() const;


    void select(TileSelectType selectType = TileSelectType::NO_FILTERED);
    void unselect();
    bool isSelected() { return m_selectType != TileSelectType::NONE; }

    bool checkForDetachableThing(TileSelectType selectType = TileSelectType::FILTERED);

    void drawTexts(Point dest);
    void setText(const std::string& text, Color color);
    std::string getText();
    void setTimer(int time, Color color);
    int getTimer();
    void setFill(Color color);
    void resetFill() { m_fill = Color::alpha; }
    bool canShoot(int distance);

    void setDrawTopAndCreature(bool draw) { m_drawTopAndCreature = draw; }
    bool drawTopAndCreature() const { return m_drawTopAndCreature; }

    void notifyAppearanceInstancesChanged() { Q_EMIT appearanceInstancesChanged(this); }

Q_SIGNALS:
    void appearanceInstancesChanged(const TWorldMapField* field);

private:
    struct TFieldStackEntry
    {
        appearances::TAppearanceInstancePtr appearance;
        uint32_t creatureId{ 0 }; // 0 = item/effect appearance on stack
    };

    appearances::TAppearanceInstancePtr makeAppearanceFor(const ThingPtr& thing) const;
    CreaturePtr stackCreature(const TFieldStackEntry& entry) const;
    int stackPriorityAt(int stackIndex) const;
    int stackPriorityFor(const TFieldStackEntry& entry) const;
    void rebuildAppearanceCache();
    bool stackEntryMatchesThing(const TFieldStackEntry& entry, const ThingPtr& thing) const;
    void setStackEntryFlag(const TFieldStackEntry& entry);
    void applyStackFlagsFromThingType(ThingType* type);
    void applyCreatureStackFlags(const tibia::creatures::TCreature& stored, ThingType* outfitType);

    void updateThingStackPos();

    void updateCreatureRangeForInsert(int16_t stackPos, const ThingPtr& thing);
    void rebuildCreatureRange();

    void setThingFlag(const ThingPtr& thing);
    void setMarkedOnStackEntry(int stackPos, const Color& color);

    void recalculateThingFlag();

    bool hasThingWithElevation() { return hasElevation() && m_thingTypeFlag & HAS_THING_WITH_ELEVATION; }
    void markHighlightedThing(const Color& color);

    std::vector<uint32_t> m_walkingCreatureIds;
    std::vector<TFieldStackEntry> m_stack;
    std::vector<appearances::TAppearanceInstancePtr> m_appearanceCache;

    std::vector<appearances::TAppearanceInstancePtr> m_effectAppearances;

    std::unique_ptr<StaticText> m_timerText;
    std::unique_ptr<StaticText> m_text;
    Color m_fill = Color::alpha;
    ticks_t m_timer = 0;

    uint32_t m_isCompletelyCovered{ 0 };
    uint32_t m_isCovered{ 0 };
    uint32_t m_thingTypeFlag{ 0 };


    TWorldMapCoordinate m_coordinate;
    mutable Position m_positionCache;

    Point m_lastDrawDest;

    uint8_t m_drawElevation{ 0 };
    uint8_t m_minimapColor{ 0 };
    uint8_t m_elevation{ 0 };

    int16_t m_firstCreatureIndex{ -1 };
    int16_t m_lastCreatureIndex{ -1 };

    int8_t m_highlightThingStackPos = -1;

    TileSelectType m_selectType{ TileSelectType::NONE };

    bool m_drawTopAndCreature{ true };
};

}
}
