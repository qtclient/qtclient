#pragma once

#include "worldmap/cacheforworldmapstoragelookupmembers.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QHash>

namespace tibia::worldmap {

class TWorldMapField;

/// Aware-range map storage (CIP: TWorldMapStorage). Wraps engine Map tiles as
/// TWorldMapField entries and emits CIP change signals when the feed reports
/// tile/window updates (see TMinimapWorldFeed / MapObserver).
class TWorldMapStorage final : public cip::tibia::worldmap::TWorldMapStorage
{
    Q_OBJECT
public:
    explicit TWorldMapStorage(QObject* parent = nullptr);
    ~TWorldMapStorage() override;

    TWorldMapField* fieldAt(const TWorldMapCoordinate& coordinate);
    TWorldMapField* fieldAt(const Position& position);
    TCacheForWorldmapStorageLookupMembers* lookupMembers() const { return m_lookupMembers; }

    /// CIP: informationAtWorldMapCoordinateHasChanged — call after a tile mutate.
    void notifyCoordinateChanged(const Position& position);
    /// CIP: storedVolumeHasChanged — call after a full map / floor window update.
    void notifyStoredVolumeChanged();

Q_SIGNALS:
    void informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate coordinate);
    void storedVolumeHasChanged();

private:
    QHash<quint64, TWorldMapField*> m_fields;
    TCacheForWorldmapStorageLookupMembers* m_lookupMembers = nullptr;

    static quint64 key(const Position& pos);
};

}
