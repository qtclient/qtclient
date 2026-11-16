#pragma once

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QByteArray>
#include <QPointer>

#include <google/protobuf/repeated_ptr_field.h>

namespace tibia {
namespace protobuf {
namespace protocol {
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
class ProtocolMapField;
class ProtocolMapObject;
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
/// Tile-description apply (clean + addThing) is routed here when live.
class TWorldmapProtocolMessageHandler final : public cip::tibia::worldmap::TWorldmapProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent = nullptr);

    /// Live handler while GameClient is up.
    static TWorldmapProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TWorldmapProtocolMessageHandler* handler);

    TWorldMapStorage* storage() const { return m_storage; }
    /// The minimap the handler keeps current: its floor and unseen fields
    /// follow the view, and every field the server describes, creates,
    /// changes or deletes is recorded. Null while there is no minimap.
    void setMinimapTileStorage(minimap::TMinimapTileStorage* minimap);
    /// The session's view: a full map centres it on the map's coordinate.
    void setViewport(TWorldMapViewport* viewport) { m_viewport = viewport; }

    /// CIP tile-description apply.
    void clearField(const Position& position);
    void applyAppearanceToField(const appearances::TAppearanceInstancePtr& appearance, const Position& position,
                                int stackPos);
    void applyCreatureIdToField(uint32_t creatureId, const Position& position, int stackPos);

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

    /// Replace a field's content with `field`'s objects; a field without objects
    /// leaves the storage. Fields outside the known area are ignored.
    void applyField(const ::tibia::protobuf::protocol::ProtocolMapField& field);
    /// Place one object at `position` / `stackPos`; false when it yields nothing to place.
    bool applyObject(const ::tibia::protobuf::protocol::ProtocolMapObject& object, const Position& position, int stackPos);

    /// CIP slot: AmbientLight (S 0x82 / 130) — native Q→H apply (skips classic bridge).
    void handleAmbientLightMessage(
        const ::tibia::protobuf::protocol::GameserverMessageAmbientLight& AmbientLightMessage);
    /// The world's time of day.
    void handleTibiaTimeMessage(const ::tibia::protobuf::protocol::GameserverMessageTibiaTime& TibiaTimeMessage);
    /// Host: the live session outlives a logout, so a new login waits for its
    /// first full map again.
    void restartSession() { m_mapKnown = false; }

Q_SIGNALS:
    void publishGameAction();

private:
    TWorldMapStorage& map() const;
    /// The view's centre moved to `center`: the storage keeps the fields of its
    /// visible area and drops the others, and effects outside it go too.
    void moveKnownArea(const Position& center);
    /// The described fields of an area, in the server's order.
    void applyArea(const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolMapField>& fields);
    /// A row or column: the view moved one field to `center`.
    void handleSlice(const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolMapField>& fields,
                     const Position& center);
    /// A floor change: the view moved to `center` on the floor above or below.
    void changeFloor(const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolMapField>& fields,
                     const Position& center, const Position& from);

    /// A described field (or one an object was created on, changed or
    /// deleted from) is recorded on the minimap.
    void updateMinimapField(const Position& position);

    TWorldMapStorage* m_storage = nullptr;
    TWorldMapViewport* m_viewport = nullptr;
    QPointer<minimap::TMinimapTileStorage> m_minimap;
    bool m_mapKnown = false;
};

}
}
