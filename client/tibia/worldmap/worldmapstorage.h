#pragma once

#include "worldmap/cacheforworldmapstoragelookupmembers.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QHash>

#include <tibia/world/world.h>

namespace tibia::worldmap {

class TWorldMapField;

/// Aware-range map storage (CIP: TWorldMapStorage). qtclient play path's field
/// owner — installs as `instance()` while the game client is live.
class TWorldMapStorage final : public cip::tibia::worldmap::TWorldMapStorage
{
    Q_OBJECT
public:
    explicit TWorldMapStorage(QObject* parent = nullptr);
    ~TWorldMapStorage() override;

    /// CIP session hub (null before/after gameclient install).
    static TWorldMapStorage* instance();
    static void setInstance(TWorldMapStorage* storage);
    /// Process fallback when no GameClient storage is installed (editor/tests).
    static TWorldMapStorage* ensureStorage();

    void removeField(const Position& position);

    TWorldMapField* fieldAt(const TWorldMapCoordinate& coordinate);
    TWorldMapField* fieldAt(const Position& position);
    /// Creates and registers the field in Map + storage hash when missing.
    TWorldMapField* getOrCreateField(const Position& position);
    TCacheForWorldmapStorageLookupMembers* lookupMembers() const { return m_lookupMembers; }

    /// CIP: informationAtWorldMapCoordinateHasChanged — call after a field mutate.
    void notifyCoordinateChanged(const Position& position);
    /// CIP: storedVolumeHasChanged — call after a full map / floor window update.
    void notifyStoredVolumeChanged();
    /// Current aware-range cuboid (central ± AwareRange), for storedVolumeHasChanged.
    TShearedWorldMapCuboid storedVolume() const;

Q_SIGNALS:
    void informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate coordinate);
    void storedVolumeHasChanged(TShearedWorldMapCuboid newStoredVolume);

private:
    QHash<quint64, TWorldMapField*> m_fields;
    TCacheForWorldmapStorageLookupMembers* m_lookupMembers = nullptr;

    static quint64 key(const Position& pos);
};

/// CIP field accessor (prefer over raw `g_map.getTile` at qtclient call sites).
inline TWorldMapField* fieldAt(const Position& position)
{
    return TWorldMapStorage::ensureStorage()->fieldAt(position);
}

} // namespace tibia::worldmap
