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

/// An object of a field and its stack position (CipSoft: appearance + stackpos + creatureId).
struct TFieldObject
{
    int stackPosition = -1;
    uint32_t creatureId = 0;
    appearances::TAppearanceInstancePtr appearance;

    explicit operator bool() const { return stackPosition >= 0; }

    uint16_t clientId() const;
};

/// One aware-range field (CIP: TWorldMapField). Owns the appearance stack (+ item/creature refs) for qtclient play.
class TWorldMapField final : public cip::tibia::worldmap::TWorldMapField
{
    Q_OBJECT
public:
    explicit TWorldMapField(QObject* parent = nullptr);
    /// The creatures still on the field leave it: they are no longer placed on a field.
    ~TWorldMapField() override;

    void setCoordinate(const TWorldMapCoordinate& coordinate)
    {
        m_coordinate = coordinate;
        syncPositionCache();
    }
    const TWorldMapCoordinate& coordinate() const { return m_coordinate; }

    Position getPosition() const { return m_coordinate.toPosition(); }
    const Position& position() const { return m_positionCache; }
    void syncPositionCache() { m_positionCache = getPosition(); }
    void clean();

    void addWalkingCreature(uint32_t creatureId);
    void removeWalkingCreature(uint32_t creatureId);

    /// The effect storage changed the effects on this field.
    void effectsChanged();
    /// Protocol item place: appearance-only stack entry (no Item::create / ItemPtr).
    void addAppearance(const appearances::TAppearanceInstancePtr& appearance, int stackPos);
    /// Phase 5.0-C: place creature from storage id + outfit appearance (no CreaturePtr).
    void addCreatureId(uint32_t creatureId, int stackPos);
    /// Remove stack entry by creature id without requiring Map `Creature`.
    bool removeCreatureById(uint32_t creatureId);
    int creatureStackPos(uint32_t creatureId) const;

    /// The stack positions in draw order: the leading bank, clip and bottom
    /// objects in stack order, then the rest of the stack from its end.
    std::vector<int> drawList() const;
    /// The object a drag from this field moves: top-down, top objects passed
    /// over, the first object that is no creature unless it cannot be moved,
    /// else the creature drawn highest above it. Bank, clip and bottom objects
    /// end the search.
    TFieldObject moveObject() const;
    /// The object a use on this field uses: the first object to force a use
    /// among the bank, clip, bottom, top objects and creatures leading the
    /// stack, else the object after them, stepping back over creatures and
    /// liquid pools; the bank when nothing else is left.
    TFieldObject useObject() const;
    /// The corpse with contents drawn highest.
    TFieldObject quickLootObject() const;
    /// The object a look looks at: the last of the leading bank, clip, bottom
    /// and top objects (a top object only when something follows it), objects
    /// with ignore look skipped; the bottom of the stack by default.
    TFieldObject lookObject() const;
    /// The object a use with targets: the first object to force a use or past
    /// the leading bank, clip, bottom and top objects (at most the last one),
    /// or the one before a liquid pool.
    TFieldObject multiUseObject() const;
    /// The creature drawn on top: the last creature of the draw list.
    TFieldObject topCreature() const;
    /// Stack position of an object, 0xff when it is not on this field.
    int stackPositionOf(const appearances::TAppearanceInstancePtr& appearance) const;
    /// The mouse can pick this field: it has a bank or bottom object without
    /// ignore look.
    bool isPickable() const;

    int getDrawElevation() const { return m_drawElevation; }
    const std::vector<uint32_t>& walkingCreatureIds() const { return m_walkingCreatureIds; }
    /// CipSoft field stack (items/effects + creature outfit appearances; gameplay items/creatures resolved on demand).
    const std::vector<appearances::TAppearanceInstancePtr>& appearances() const { return m_stack; }
    appearances::TAppearanceInstancePtr appearanceAt(int stackPos) const;
    uint16_t itemClientIdAt(int stackPos) const;
    uint32_t stackCreatureId(int stackPos) const;
    bool removeThingAtStackPos(int stackPos);
    /// Takes the creature instance at `stackPos` off the field; null when
    /// none is there.
    appearances::TAppearanceInstancePtr takeCreatureInstance(int stackPos);
    /// Advances every instance's animation to `nowMs` (CIP v45).
    void updateAnimations(qint64 nowMs);
    /// Refresh outfit/direction appearance entries when the live creature changes look.
    void syncCreatureStackAppearance(uint32_t creatureId);
    /// The graphical effects on this field, from the effect storage.
    const std::vector<appearances::TAppearanceInstancePtr>& effectAppearances() const;
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
    /// Speed of the first ground item in the stack, 255 without one.
    int movementGroundSpeed() const;
    /// A bank object lies here.
    bool hasBank() const;
    /// The creatures on the field, in stack order.
    std::vector<std::shared_ptr<tibia::creatures::TCreature>> creatures() const;
    /// An object of the field is a bottom object.
    bool hasBottom() const;
    /// An object of the field blocks sight (unsight).
    bool hasUnsight() const;
    /// An object of the field is never hidden.
    bool hasDontHide() const;
    /// An object of the field is translucent.
    bool hasTranslucent() const;
    /// An unpassable object or another unpassable creature stands here.
    bool blocksMovement() const;
    /// An object that is no creature cannot be passed.
    bool hasUnpassableObject() const;
    /// The field hides the floor below it (updated with the stack).
    bool isOpaque() const { return m_opaque; }
    /// Height of the stacked objects in pixels, at most 24 (updated with the stack).
    uint8_t objectElevation() const { return m_objectElevation; }
    /// The field blocks sight (updated with the stack).
    bool blocksSight() const { return m_blocksSight; }
    /// The light of the highest shining object; intensity 0 without one.
    Light light() const;
    /// A type and creature id per stack position (flag scans).
    struct TStackView
    {
        const TAppearanceType* type = nullptr;
        uint32_t creatureId = 0;
    };
    std::vector<TStackView> stackView() const;
    uint8_t getMinimapColorByte();
    int getThingCount() { return static_cast<int>(m_stack.size()); }
    int thingCount() const { return static_cast<int>(m_stack.size()); }

    bool isWalkable(bool ignoreCreatures = false) const;
    /// Whether the field can be stepped onto, from its stack in order: 0 free,
    /// 1 blocked, 2 unsure.
    int walkState() const;
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
    bool hasBlockingBottomItem() const;

    bool hasEffect() const;
    bool hasGround() const;
    bool hasTopGround(const bool ignoreBorder = false) const;

    bool hasCreatures() const { return (m_thingTypeFlag & HAS_CREATURE) != 0; }
    bool hasTopItem() const { return m_thingTypeFlag & HAS_TOP_ITEM; }
    bool hasCommonItem() const { return m_thingTypeFlag & HAS_COMMON_ITEM; }
    bool hasBottomItem() const { return m_thingTypeFlag & HAS_BOTTOM_ITEM; }
    bool hasDisplacement() const { return m_thingTypeFlag & HAS_DISPLACEMENT; }
    bool hasLight() const { return m_thingTypeFlag & HAS_LIGHT; }
    bool hasTallThings() const { return m_thingTypeFlag & HAS_TALL_THINGS; }
    bool hasWideThings() const { return m_thingTypeFlag & HAS_WIDE_THINGS; }
    bool limitsFloorsView(bool isFreeView = false);

    bool canShade() const { return isFullyOpaque() || hasGround() || hasTopGround() || isFullGround(); }
    bool canErase()
    {
        return !isDrawable() && m_minimapColor == 0
            ;
    }

    bool hasElevation(const int elevation = 1) { return m_elevation >= elevation; }
    void notifyAppearanceInstancesChanged()
    {
        updateElevation();
        updateViewFlags();
        Q_EMIT appearanceInstancesChanged(this);
    }

Q_SIGNALS:
    void appearanceInstancesChanged(const TWorldMapField* field);

private:
    /// The stack entry at `stackIndex` as an object, at its stack position.
    TFieldObject fieldObject(int stackIndex) const;
    /// Where an object sorts on the stack: 0 bank, 1 clip, 2 bottom, 3 top,
    /// 4 creature, 5 any other object.
    static int stackOrder(const appearances::TAppearanceInstancePtr& entry);
    /// The stack position an object without one takes.
    int insertionIndex(const appearances::TAppearanceInstancePtr& entry) const;
    /// Sets each object's elevation and the hangables' hook side from the stack.
    void updateElevation();
    /// Recomputes isOpaque and blocksSight from the stack.
    void updateViewFlags();
    /// Takes the stack entry at `stackIndex` off the field without touching a
    /// creature's placed-on-field flag (the stack's overflow).
    void eraseStackEntry(int stackIndex);
    void setStackEntryFlag(const appearances::TAppearanceInstancePtr& entry);
    void applyStackFlagsFromThingType(TAppearanceType* type);
    void applyCreatureStackFlags(const tibia::creatures::TCreature& stored, TAppearanceType* outfitType);

    void updateThingStackPos();

    void rebuildCreatureRange();
    void recalculateThingFlag();

    bool hasThingWithElevation() { return hasElevation() && m_thingTypeFlag & HAS_THING_WITH_ELEVATION; }
    std::vector<uint32_t> m_walkingCreatureIds;
    /// The field's appearance instances in stack order (a creature's is linked to it).
    std::vector<appearances::TAppearanceInstancePtr> m_stack;



    uint32_t m_isCompletelyCovered{ 0 };
    uint32_t m_isCovered{ 0 };
    uint32_t m_thingTypeFlag{ 0 };


    TWorldMapCoordinate m_coordinate;
    mutable Position m_positionCache;


    uint8_t m_drawElevation{ 0 };
    uint8_t m_minimapColor{ 0 };
    uint8_t m_elevation{ 0 };
    bool m_opaque{ false };
    uint8_t m_objectElevation{ 0 };
    bool m_blocksSight{ false };

    int16_t m_firstCreatureIndex{ -1 };
    int16_t m_lastCreatureIndex{ -1 };



};

}
}
