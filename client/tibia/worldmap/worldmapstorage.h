#pragma once

#include "creatures/creaturemovementspeedcalculator.h"
#include "worldmap/cacheforworldmapstoragelookupmembers.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"

#include <client/declarations.h>
#include <client/position.h>

#include "worldmap/worldmapsessionstate.h"

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace worldmap {

class TWorldMapField;

/// Aware-range map storage (CIP: TWorldMapStorage). qtclient play path's field
/// owner — installs as `instance()` while the game client is live.
class TWorldMapStorage final : public cip::tibia::worldmap::TWorldMapStorage
{
    Q_OBJECT
public:
    /// `speedCalculator`: the session's (CIP passes it in); the storage keeps
    /// one of its own when none is given (the fallback storage).
    explicit TWorldMapStorage(creatures::TCreatureMovementSpeedCalculator* speedCalculator = nullptr,
                              QObject* parent = nullptr);
    ~TWorldMapStorage() override;

    /// CIP session hub (null before/after gameclient install).
    static TWorldMapStorage* instance();
    static void setInstance(TWorldMapStorage* storage);
    /// Process fallback when no GameClient storage is installed (editor/tests).
    static TWorldMapStorage* ensureStorage();

    /// Drops the field at `position` (no notification).
    void removeField(const Position& position);
    /// Drops every field (world leave / session clean).
    void clearAllFields();
    /// The fields of one floor (every floor for -1), in coordinate order.
    WorldMapFieldList fieldList(int8_t floor = -1) const;
    /// The field at `coordinate`, shared.
    std::shared_ptr<TWorldMapField> field(const TWorldMapCoordinate& coordinate) const;

    TWorldMapField* fieldAt(const TWorldMapCoordinate& coordinate);
    TWorldMapField* fieldAt(const Position& position);
    /// The storage holds a field at `coordinate`.
    bool hasField(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds an unpassable object; false without a field.
    bool hasUnpassableObjectAt(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds a bank object; false without a field.
    bool hasBankAt(const TWorldMapCoordinate& coordinate);
    /// A creature stands on the field at `coordinate`.
    bool hasCreatureAt(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds a translucent object; false without a field.
    bool hasTranslucentAt(const TWorldMapCoordinate& coordinate);
    /// Nothing opaque covers the field from above: it lies on or above the highest
    /// floor of its column that holds an opaque field. A sight-blocking field looks
    /// at the column one field down-right. Underground fields never are.
    bool isUnderOpenSkyAt(const TWorldMapCoordinate& coordinate);
    /// The highest floor (1..6) of the column at (x, y) holding an opaque field, 7 when
    /// none does, and 0 when floor 0 is opaque itself.
    int highestOpaqueFloorAt(int x, int y);
    /// The field at `position`, created when the position lies in the stored
    /// volume (null outside it). A new field announces its coordinate.
    TWorldMapField* getOrCreateField(const Position& position);
    /// Every stored field with its coordinate, in coordinate order (CIP v17).
    std::vector<std::pair<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>>> fields() const;
    /// The stored fields inside `cuboid` (CIP v18).
    std::vector<std::pair<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>>> fieldsIn(const TShearedWorldMapCuboid& cuboid) const;
    /// The creatures standing on the fields inside `cuboid` (CIP v19).
    std::vector<std::shared_ptr<creatures::TCreature>> creaturesIn(const TShearedWorldMapCuboid& cuboid) const;
    /// Stores `field` at `coordinate` (null drops the stored field) when the
    /// coordinate is inside the stored volume, and reports the coordinate (CIP v21).
    void setField(const TWorldMapCoordinate& coordinate, std::shared_ptr<TWorldMapField> field);
    /// Drops every field outside the stored volume; `onRemoved` runs for each
    /// coordinate after its field left, before the change is announced.
    void removeFieldsOutsideStoredVolume(const std::function<bool(const Position&)>& isStored,
                                         const std::function<void(const Position&)>& onRemoved);

    creatures::TCreatureMovementSpeedCalculator& speedCalculator() { return *m_speedCalculator; }
    /// Speed of the field's ground; 255 where there is no ground.
    int groundSpeedAt(const Position& position);
    /// Accumulated elevation of the field's items; 0 where there is no field.
    /// The pixels the objects of a field lift a creature by; 0 without the field (CIP v26).
    int elevationAt(const Position& position);
    /// Advances the animations of every stored field to `nowMs` (CIP v36).
    void updateAnimations(qint64 nowMs);
    /// Animate `creature` stepping from `from` to `to`. Anything other than a
    /// single step drops its motion instead.
    /// The appearance instance at `stackPos` of the field at `coordinate`; null
    /// without the field or the position.
    appearances::TAppearanceInstancePtr appearanceInstanceAt(const TWorldMapCoordinate& coordinate, int stackPos);
    /// The creature instance at `stackPos`; throws std::runtime_error with the
    /// field's stack when another instance or none is there.
    appearances::TAppearanceInstancePtr creatureInstanceAt(const TWorldMapCoordinate& coordinate, int stackPos);
    /// Puts a creature's instance on the field at `target` and the creature there;
    /// throws std::runtime_error without the field or a linked creature.
    void placeCreatureInstance(const appearances::TAppearanceInstancePtr& instance, const TWorldMapCoordinate& target);
    /// Moves the creature at `stackPos` of `from` to `target`: it turns towards
    /// the step and starts walking it. False when no creature is there.
    bool moveCreatureInstance(const TWorldMapCoordinate& from, int stackPos, const TWorldMapCoordinate& target);
    void moveCreature(creatures::TCreature& creature, const Position& from, const Position& to, int elevationFrom,
                      int elevationTo);

    TWorldMapSessionState& sessionState() { return m_sessionState; }
    const TWorldMapSessionState& sessionState() const { return m_sessionState; }

    /// CIP field mutates — protocol / handlers prefer sessionState / storage APIs at call sites.
    void clearField(const Position& position);
    bool placeAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& position,
                         int stackPos = -1);
    bool placeCreatureId(uint32_t creatureId, const Position& position, int stackPos = -1);
    bool removeCreatureFromFields(uint32_t creatureId, const Position& fieldPos = {},
                                  bool fireLocalDisappear = true);
    bool applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack, const Position& newPos,
                           bool byId);

    /// CIP: informationAtWorldMapCoordinateHasChanged — call after a field mutate.
    void notifyCoordinateChanged(const Position& position);
    /// The storage keeps the fields of the visible area around `center` (the
    /// view's centre) and drops every other field; storedVolumeHasChanged when
    /// the volume moved.
    void setStoredVolume(const Position& center);
    /// Current aware-range cuboid (central ± AwareRange), widened by the
    /// stored-volume margins, for storedVolumeHasChanged.
    TShearedWorldMapCuboid storedVolume() const;
    /// CIP v38: the stored volume reaches `left` fields further west and
    /// `right` further east than the aware range (0 unless set).
    void setHorizontalStoredVolumeMargins(int left, int right)
    {
        m_storedVolumeMarginLeft = left;
        m_storedVolumeMarginRight = right;
    }
    /// CIP v39: the same north (`top`) and south (`bottom`).
    void setVerticalStoredVolumeMargins(int top, int bottom)
    {
        m_storedVolumeMarginTop = top;
        m_storedVolumeMarginBottom = bottom;
    }

    /// Dynamic overlay peels (qtclient-bridge dual-write).
    void addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance);
    void addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& position);
    void addStaticText(const StaticTextPtr& text, const Position& position);
    bool removeStaticText(const StaticTextPtr& text);
    void addAnimatedText(const AnimatedTextPtr& text, const Position& position);
    bool removeAnimatedText(const AnimatedTextPtr& text);

Q_SIGNALS:
    void informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate coordinate);
    void storedVolumeHasChanged(TShearedWorldMapCuboid newStoredVolume);

private:
    /// A field's objects changed: its coordinate's information did.
    void fieldAppearanceInstancesChanged(const TWorldMapField* field);

    std::map<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>> m_fields;
    TWorldMapSessionState m_sessionState;
    creatures::TCreatureMovementSpeedCalculator m_ownSpeedCalculator;
    creatures::TCreatureMovementSpeedCalculator* m_speedCalculator = nullptr;
    int m_storedVolumeMarginLeft = 0;
    int m_storedVolumeMarginRight = 0;
    int m_storedVolumeMarginTop = 0;
    int m_storedVolumeMarginBottom = 0;
};

/// CIP field accessor (prefer over legacy Map tile getters at qtclient call sites).
inline TWorldMapField* fieldAt(const Position& position)
{
    return TWorldMapStorage::ensureStorage()->fieldAt(position);
}

/// Creates the field in storage when missing.
inline TWorldMapField* getOrCreateField(const Position& position)
{
    return TWorldMapStorage::ensureStorage()->getOrCreateField(position);
}

/// Floor field list for minimap / feed.
WorldMapFieldList fieldsOnFloor(int8_t floor = -1);
/// Per-floor missile appearances for worldmap renderer.
const std::vector<tibia::appearances::TAppearanceInstancePtr>& floorMissiles(uint8_t z);



}
}
