#pragma once

#include "creatures/creaturemovementspeedcalculator.h"
#include "worldmap/cacheforworldmapstoragelookupmembers.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QHash>

#include "worldmap/tileblock.h"
#include "worldmap/worldmapsessionstate.h"

#include <functional>
#include <unordered_map>
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

    void removeField(const Position& position);
    /// Drop hash + spatial slots on world leave / session `clean` (QObject fields stay until storage dtor).
    void clearAllFields();
    void clearSpatialIndex();
    WorldMapFieldList spatialFieldList(int8_t floor = -1);

    TWorldMapField* fieldAt(const TWorldMapCoordinate& coordinate);
    TWorldMapField* fieldAt(const Position& position);
    /// The storage holds a field at `coordinate`.
    bool hasField(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds an unpassable object; false without a field.
    bool hasUnpassableObjectAt(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds a bank object; false without a field.
    bool hasBankAt(const TWorldMapCoordinate& coordinate);
    /// The field at `coordinate` holds a translucent object; false without a field.
    bool hasTranslucentAt(const TWorldMapCoordinate& coordinate);
    /// Sole field create path — owns `m_fields` and spatial `tileBlocks`.
    TWorldMapField* getOrCreateField(const Position& position);
    /// Spatial block slot only; does not read `m_fields`.
    TWorldMapField* fieldInSpatialIndex(const Position& position);
    void bindFieldToSpatialIndex(const Position& position, TWorldMapField* field);
    void removeFieldFromSpatialIndex(const Position& position);
    /// Drop blocks outside `isAware`; `onRemoved` runs after block slot is cleared (Map notifies).
    void pruneUnawareSpatialFields(const std::function<bool(const Position&)>& isAware,
                                   const std::function<void(const Position&)>& onRemoved);

    creatures::TCreatureMovementSpeedCalculator& speedCalculator() { return *m_speedCalculator; }
    /// Speed of the field's ground; 255 where there is no ground.
    int groundSpeedAt(const Position& position);
    /// Accumulated elevation of the field's items; 0 where there is no field.
    int elevationAt(const Position& position);
    /// Animate `creature` stepping from `from` to `to`. Anything other than a
    /// single step drops its motion instead.
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
    /// Current aware-range cuboid (central ± AwareRange), for storedVolumeHasChanged.
    TShearedWorldMapCuboid storedVolume() const;

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
    void ensureSpatialFloorsSized();
    static uint16_t blockIndex(const Position& pos);

    QHash<quint64, TWorldMapField*> m_fields;
    std::vector<std::unordered_map<uint32_t, TileBlock>> m_spatialFloors;
    TWorldMapSessionState m_sessionState;
    creatures::TCreatureMovementSpeedCalculator m_ownSpeedCalculator;
    creatures::TCreatureMovementSpeedCalculator* m_speedCalculator = nullptr;

    static quint64 key(const Position& pos);
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
