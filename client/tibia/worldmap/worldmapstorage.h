#pragma once

#include "creatures/creaturemovementspeedcalculator.h"
#include "worldmap/cacheforworldmapstoragelookupmembers.h"
#include "worldmap/coordinates.h"
#include "game/currentgamesession.h"

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"

#include <client/const.h>
#include <client/declarations.h>
#include <client/position.h>
#include <client/staticdata.h>

#include <tibia/schedule/schedule.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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
class TWaypointCosts;
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
    /// The session's storage: steps take their duration from `speedCalculator`
    /// (one of its own when none is given) and start at `clock`'s time; no
    /// fields and an empty stored volume yet. The owning session resets the
    /// aware range (TGameSessionBase::initializeWorldMapSessionState).
    TWorldMapStorage(std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> speedCalculator,
                     std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TWorldMapStorage() override;

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

    /// The field at `position` holds `objects` in their order (creatures are
    /// their placeholders); a field left without objects leaves the storage.
    void setFieldObjects(const Position& position, const std::vector<appearances::TAppearanceInstancePtr>& objects);
    /// Puts an object on the field at `position`: at `stackPos`, or where its
    /// kind sorts for 0xff (or a negative position).
    bool insertObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position, int stackPos);
    /// The object at `stackPos` of the field at `position` makes way for `object`.
    bool replaceObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position, int stackPos);
    /// Takes the object at `stackPos` off the field at `position`; null when none is there.
    appearances::TAppearanceInstancePtr removeObjectAt(const Position& position, int stackPos);
    /// Takes the creature off the field `fieldPos` names, else off the field it
    /// stands on. `fireLocalDisappear`: DeleteOnMap and unaware fields only.
    bool removeCreatureFromFields(uint32_t creatureId, const Position& fieldPos = {},
                                  bool fireLocalDisappear = true);
    /// MoveCreature. `byId`: the message names the creature instead of its field and slot.
    bool applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack, const Position& newPos,
                           bool byId);
    /// Takes every creature off the fields and out of the creature storage.
    void cleanDynamicThings();
    /// The creatures leave, then every field goes (world leave).
    void clean();

    /// Host: the known area around the central position. A field is known when,
    /// projected onto the central floor, it lies within the aware range and its
    /// floor is between firstAwareFloor() and lastAwareFloor().
    bool isAwareOfPosition(const Position& position) const { return isAwareOfPosition(position, m_awareRange); }
    bool isAwareOfPosition(const Position& position, const AwareRange& range) const;
    /// The aware range; changing it drops what leaves the known area.
    AwareRange awareRange() const { return m_awareRange; }
    void setAwareRange(const AwareRange& range);
    /// The aware range of the configured viewport.
    void resetAwareRange();
    Position centralPosition() const { return m_centralPosition; }
    uint8_t firstAwareFloor() const;
    uint8_t lastAwareFloor() const;

    /// Host: the local player's field as the map last placed it; its creature's
    /// position takes over once the creature stands somewhere.
    Position localPlayerPosition() const { return m_localPlayerPosition; }
    /// A new character session: the player is off the map and has no field yet.
    void resetLocalPlayer();
    /// The map placed the local player on `position` (onPositionChange when it moved).
    void setLocalPlayerPosition(const Position& position);

    /// Host: the server's world light as it arrived (the pack's raycast lighting
    /// reads it; TAmbientLightStorage shows the interpolated one).
    Light worldLight() const { return m_worldLight; }
    /// Publishes onWorldLightChange when intensity or colour differ.
    void setWorldLight(uint8_t intensity, uint8_t color);

    /// Host: bumped when an opaque field changes (a field is cleaned); the
    /// renderer's floor cache follows it. Cover caches are cleared per cell.
    uint32_t coverRevision() const { return m_coverRevision.load(std::memory_order_relaxed); }
    /// Bumped when an item is added to or removed from a field, or a field is cleaned.
    uint32_t itemRevision() const { return m_itemRevision.load(std::memory_order_relaxed); }
    /// One world map build: memoize opaque / top-ground probes shared across FOV cover walks.
    void beginCoverProbeBatch();
    void endCoverProbeBatch();
    /// Drops the pending static dirty fields (cover-flood coalescing counter);
    /// returns how many were discarded.
    size_t clearStaticDirtyTiles();
    /// A field above `position` (from floor `firstFloor` on) covers it.
    /// `isLoading`: a field on the way is still loading (no answer yet).
    bool isCovered(const Position& position, bool& isLoading, uint8_t firstFloor = 0);
    bool isCompletelyCovered(const Position& position, bool& isLoading, uint8_t firstFloor = 0);

    /// Host (pack travel path finder): fields the path finder keeps off.
    void setPathAvoid(const Position& position, bool avoid);
    void clearPathAvoids();
    bool isPathAvoided(const Position& position) const { return m_pathAvoids.contains(position); }
    const std::unordered_set<Position, Position::Hasher>& pathAvoids() const { return m_pathAvoids; }
    /// Indexed8 waypoint snapshot factory (CIP TWaypointCostMap), set from
    /// TGameClient while the CipSoft minimap is live; cleared on world leave.
    using WaypointCostFactory = std::function<std::shared_ptr<minimap::TWaypointCosts>()>;
    const WaypointCostFactory& waypointCostFactory() const { return m_waypointCostFactory; }
    void setWaypointCostFactory(WaypointCostFactory factory) { m_waypointCostFactory = std::move(factory); }
    void clearWaypointCostFactory() { m_waypointCostFactory = {}; }

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

    /// A missile goes to the effect storage.
    void addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance);
    /// A magic effect goes to the effect storage; it shows on its field even
    /// before the field has content.
    void addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& position);

Q_SIGNALS:
    void informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate coordinate);
    void storedVolumeHasChanged(TShearedWorldMapCuboid newStoredVolume);

private:
    /// A field's objects changed: its coordinate's information did.
    void fieldAppearanceInstancesChanged(const TWorldMapField* field);
    /// `position` lies in the stored volume (the aware range while no volume
    /// was set).
    bool isInStoredVolume(const Position& position) const;

    /// Host: the field at `position` changed (`creature`: a creature moved on
    /// it, not the field content). Bumps the revisions, clears the cover
    /// caches that look through it and announces the coordinate.
    void fieldUpdated(const Position& position, bool creature, tibia::client::Operation operation);
    /// The field at `position` loses its objects; an empty field leaves the storage.
    void cleanField(const Position& position);
    /// A field at `position` took `object`: the field announces the change, and
    /// the local player's placeholder places the local player there.
    void objectPlaced(const appearances::TAppearanceInstancePtr& object, const Position& position);
    /// The local player's creature went onto a field: a field it did not step
    /// to from the last one ends the motion and the walk goal.
    void localPlayerAppeared();
    /// The local player's creature left its field; unless it reappears before
    /// the event loop turns, it counts as off the map and its step stops.
    void localPlayerDisappeared();
    /// Creatures and fields outside the known area go.
    void removeUnawareThings();
    void setCentralPosition(const Position& center);
    /// Drop isCompletelyCovered / isCovered bits for fields that look up through `position`.
    void invalidateCoverCachesFrom(const Position& position);
    /// Wipe every stored field's cover bits (stream-flood coalesce).
    void invalidateAllCoverCaches();
    /// Packed cover-probe flags for one build (see beginCoverProbeBatch).
    uint8_t probeCoverTile(const Position& position);

    std::map<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>> m_fields;
    creatures::TCreatureMovementSpeedCalculator m_ownSpeedCalculator;
    creatures::TCreatureMovementSpeedCalculator* m_speedCalculator = nullptr;
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_sharedSpeedCalculator;
    std::shared_ptr<shared::IClockService> m_clock;
    int m_storedVolumeMarginLeft = 0;
    int m_storedVolumeMarginRight = 0;
    int m_storedVolumeMarginTop = 0;
    int m_storedVolumeMarginBottom = 0;
    /// The volume setStoredVolume(volume, withMargins) set; unset for the live
    /// session's storage.
    std::optional<TShearedWorldMapCuboid> m_storedVolume;

    // Host: the known area, the local player's field, the server light, the
    // renderer's revisions and cover probes, and the pack path finder's state.
    Position m_centralPosition;
    AwareRange m_awareRange;
    Position m_localPlayerPosition;
    /// The field the player left (the jump test); cleared once it stays off the map.
    Position m_localPlayerLastField;
    bool m_localPlayerOffMap{ true };
    tibia::schedule::TScheduledTaskPtr m_localPlayerLeave;
    Light m_worldLight;
    bool m_removingUnawareThings{ false };
    std::atomic<uint32_t> m_coverRevision{ 0 };
    std::atomic<uint32_t> m_itemRevision{ 0 };
    std::vector<Position> m_staticDirty;
    static constexpr size_t MaxStaticDirty = 4096;
    /// Opaque stream hit MaxStaticDirty: one full cover wipe is deferred instead of N column walks.
    bool m_coverInvalidateFlood{ false };
    /// Frame-local memo: seen|empty|opaque|topGround|loading (see probeCoverTile).
    bool m_coverProbeActive{ false };
    std::unordered_map<Position, uint8_t, Position::Hasher> m_coverProbe;
    std::unordered_set<Position, Position::Hasher> m_pathAvoids;
    WaypointCostFactory m_waypointCostFactory;
};

/// CIP field accessor (prefer over legacy Map tile getters at qtclient call sites).
inline TWorldMapField* fieldAt(const Position& position)
{
    TWorldMapStorage* storage = game::currentWorldMapStorage();
    return storage ? storage->fieldAt(position) : nullptr;
}

/// Creates the field in storage when missing.
inline TWorldMapField* getOrCreateField(const Position& position)
{
    TWorldMapStorage* storage = game::currentWorldMapStorage();
    return storage ? storage->getOrCreateField(position) : nullptr;
}

/// Floor field list for minimap / feed.
WorldMapFieldList fieldsOnFloor(int8_t floor = -1);
/// Per-floor missile appearances for worldmap renderer.
const std::vector<tibia::appearances::TAppearanceInstancePtr>& floorMissiles(uint8_t z);



}
}
