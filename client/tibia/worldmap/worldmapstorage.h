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
#include <optional>
#include <utility>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace shared {
class IClockService;
}
namespace input {
class TLocationWorldMapCoordinateWithStackPositionContext;
}
namespace minimap {
struct TMinimapDataOfOneField;
class TWaypointSectors;
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
    /// The session's storage: steps take their duration from `speedCalculator`
    /// and start at `clock`'s time; no fields and an empty stored volume yet.
    TWorldMapStorage(std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> speedCalculator,
                     std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
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
    /// The appearance instance a game action's field and stack position name. The
    /// field must be stored; a stack position past its stack throws std::out_of_range.
    appearances::TAppearanceInstance* appearanceInstanceAt(const input::TLocationWorldMapCoordinateWithStackPositionContext& context);
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

    /// The field at `position` holds `objects` in their order; without
    /// objects it leaves the storage (see TWorldMapSessionState::setFieldObjects).
    void setFieldObjects(const Position& position, const std::vector<appearances::TAppearanceInstancePtr>& objects);
    /// Puts an object on the field at `position`: at `stackPos`, or where its
    /// kind sorts for 0xff.
    bool insertObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position, int stackPos);
    /// The object at `stackPos` of the field at `position` makes way for `object`.
    bool replaceObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position, int stackPos);
    /// Takes the object at `stackPos` off the field at `position`; null when none is there.
    appearances::TAppearanceInstancePtr removeObjectAt(const Position& position, int stackPos);
    bool removeCreatureFromFields(uint32_t creatureId, const Position& fieldPos = {},
                                  bool fireLocalDisappear = true);
    bool applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack, const Position& newPos,
                           bool byId);

    /// CIP: informationAtWorldMapCoordinateHasChanged — call after a field mutate.
    void notifyCoordinateChanged(const Position& position);
    /// The storage keeps the fields of the visible area around `center` (the
    /// view's centre) and drops every other field; storedVolumeHasChanged when
    /// the volume moved. Host: the live session's storage places its volume
    /// through the session state's aware range.
    void setStoredVolume(const Position& center);
    /// The storage keeps the fields inside `volume` (widened by the stored-volume
    /// margins when `withMargins`) and drops every other field;
    /// storedVolumeHasChanged when the volume changed. From then on fields are
    /// created and stored inside this volume only.
    void setStoredVolume(const TShearedWorldMapCuboid& volume, bool withMargins);
    /// The stored volume: the volume set, or the aware-range cuboid (central ±
    /// AwareRange) widened by the stored-volume margins.
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

    /// The minimap data of the field at `coordinate`; the data of an unknown,
    /// impassable and avoided field when the storage holds none.
    minimap::TMinimapDataOfOneField minimapDataOfField(const TWorldMapCoordinate& coordinate) const;
    /// The minimap data of every stored field of floor `z`.
    std::map<TWorldMapCoordinate, minimap::TMinimapDataOfOneField> minimapDataOfFloor(int z) const;
    /// The waypoint costs of the stored fields on `start`'s floor: each field with
    /// a ground speed whose objects do not block movement costs its ground speed;
    /// a field the path finder keeps off (an object to avoid or a creature) is
    /// left out unless it is `start` or `goal`.
    std::shared_ptr<minimap::TWaypointSectors> waypointCostMap(const TWorldMapCoordinate& start,
                                                               const TWorldMapCoordinate& goal) const;
    /// Fills `data` from `field`: its automap colour, its ground speed (at most
    /// 250), whether an unpassable object lies on it and whether the path finder
    /// keeps off it (a walk state other than free, an object to avoid or a creature).
    void fillMinimapData(const TWorldMapField& field, minimap::TMinimapDataOfOneField& data) const;

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
    /// `position` lies in the stored volume (the session state's aware range
    /// while no volume was set).
    bool isInStoredVolume(const Position& position) const;

    std::map<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>> m_fields;
    TWorldMapSessionState m_sessionState;
    creatures::TCreatureMovementSpeedCalculator m_ownSpeedCalculator;
    creatures::TCreatureMovementSpeedCalculator* m_speedCalculator = nullptr;
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_sharedSpeedCalculator;
    /// Host: without a clock (the raw-pointer constructor) steps start at the frame clock.
    std::shared_ptr<shared::IClockService> m_clock;
    int m_storedVolumeMarginLeft = 0;
    int m_storedVolumeMarginRight = 0;
    int m_storedVolumeMarginTop = 0;
    int m_storedVolumeMarginBottom = 0;
    /// The volume setStoredVolume(volume, withMargins) set; unset for the live
    /// session's storage.
    std::optional<TShearedWorldMapCuboid> m_storedVolume;
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
