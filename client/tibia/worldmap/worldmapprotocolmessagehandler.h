#pragma once

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"
#include "worldmap/coordinates.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QByteArray>
#include <QPointer>

namespace tibia {
namespace protobuf {
namespace protocol {
class AppearanceInstance;
class GameserverMessageAmbientLight;
class GameserverMessageBottomFloor;
class GameserverMessageBottomRow;
class GameserverMessageChangeOnMap;
class GameserverMessageCreateOnMap;
class GameserverMessageDeleteOnMap;
class GameserverMessageFieldData;
class GameserverMessageFullMap;
class GameserverMessageLeftColumn;
class GameserverMessageRightColumn;
class GameserverMessageTibiaTime;
class GameserverMessageTopFloor;
class GameserverMessageTopRow;
class MapArea;
}
}
}

namespace tibia {
namespace minimap {
class TMinimapTileStorage;
}
namespace worldmap {

class TWorldMapStorage;
class TWorldMapViewport;

/// CIP name for map/ambient protocol ingress (TWorldmapProtocolMessageHandler).
/// Tile-description apply (clean + addThing) goes to the session's map.
class TWorldmapProtocolMessageHandler final : public cip::tibia::worldmap::TWorldmapProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent = nullptr);

    TWorldMapStorage* storage() const { return m_storage; }
    /// The minimap the handler keeps current: its floor and unseen fields
    /// follow the view, and every field the server describes, creates,
    /// changes or deletes is recorded. Null while there is no minimap.
    void setMinimapTileStorage(minimap::TMinimapTileStorage* minimap);
    /// The session's view: a full map centres it on the map's coordinate.
    void setViewport(TWorldMapViewport* viewport) { m_viewport = viewport; }

    void handleFullMapMessage(const ::tibia::protobuf::protocol::GameserverMessageFullMap& message);
    void handleTopRowMessage(const ::tibia::protobuf::protocol::GameserverMessageTopRow& message);
    void handleRightColumnMessage(const ::tibia::protobuf::protocol::GameserverMessageRightColumn& message);
    void handleBottomRowMessage(const ::tibia::protobuf::protocol::GameserverMessageBottomRow& message);
    void handleLeftColumnMessage(const ::tibia::protobuf::protocol::GameserverMessageLeftColumn& message);
    void handleTopFloorMessage(const ::tibia::protobuf::protocol::GameserverMessageTopFloor& message);
    void handleBottomFloorMessage(const ::tibia::protobuf::protocol::GameserverMessageBottomFloor& message);
    void handleFieldDataMessage(const ::tibia::protobuf::protocol::GameserverMessageFieldData& message);
    void handleCreateOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageCreateOnMap& message);
    void handleChangeOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageChangeOnMap& message);
    void handleDeleteOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageDeleteOnMap& message);

    /// The instance a map object at `coordinate` stands for: a creature's data
    /// is applied, the creature placed at the field and its placeholder
    /// returned; an object's instance takes the field's patterns. Null when the
    /// object yields nothing.
    appearances::TAppearanceInstancePtr applyObject(const ::tibia::protobuf::protocol::AppearanceInstance& object,
                                                    const TWorldMapCoordinate& coordinate);

    /// CIP slot: AmbientLight (S 0x82 / 130) — native Q→H apply (skips classic bridge).
    void handleAmbientLightMessage(
        const ::tibia::protobuf::protocol::GameserverMessageAmbientLight& AmbientLightMessage);
    /// The world's time of day.
    void handleTibiaTimeMessage(const ::tibia::protobuf::protocol::GameserverMessageTibiaTime& TibiaTimeMessage);

Q_SIGNALS:

private:
    TWorldMapStorage& map() const;
    /// The central position moved: the storage keeps the fields of its visible
    /// area and drops the others, effects outside it go too, and the minimap
    /// follows its floor.
    void moveKnownArea();
    void setCrashReportPosition(const Position& center);
    /// The described fields of `area`, the `index`th of them at the `index`th
    /// field of `volume` (skipped fields counted): each field the storage
    /// holds takes the field's objects (none: the field is dropped). Throws
    /// when the fields do not cover the volume exactly.
    void applyArea(const ::tibia::protobuf::protocol::MapArea& area, const TShearedWorldMapCuboid& volume,
                   const TWorldMapCentralPosition& central);
    /// A row, column or floor change: the area the move of the central
    /// position added.
    void handleSlice(const ::tibia::protobuf::protocol::MapArea& area, const TShearedWorldMapCuboid& volume);
    /// The central position with the session's aware area.
    TWorldMapCentralPosition centralPosition() const;

    /// A described field (or one an object was created on, changed or
    /// deleted from) is recorded on the minimap.
    void updateMinimapField(const Position& position);

    TWorldMapStorage* m_storage = nullptr;
    TWorldMapViewport* m_viewport = nullptr;
    QPointer<minimap::TMinimapTileStorage> m_minimap;
    bool m_mapKnown = false;
    /// The centre of the map messages (CIP: the handler's central position).
    TWorldMapCoordinate m_center;
};

}
}
