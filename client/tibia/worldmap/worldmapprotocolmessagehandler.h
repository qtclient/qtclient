#pragma once

#include <cip_gen.h>

#include "appearances/appearanceinstance.h"

#include <client/declarations.h>
#include <client/position.h>
#include <tibia/worldmap/mapobserver.h>

#include <QByteArray>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageAmbientLight;
}
}
}

namespace tibia {
namespace worldmap {

class TWorldMapStorage;

/// CIP name for map/ambient protocol ingress (TWorldmapProtocolMessageHandler).
/// Binary decode stays in ProtocolGame::setTileDescription (classic qtclient wire);
/// tile-description *apply* (clean + addThing) is routed here when live.
/// MapObserver keeps CIP TWorldMapStorage on the notify path (minimap is sibling).
class TWorldmapProtocolMessageHandler final : public cip::tibia::worldmap::TWorldmapProtocolMessageHandler,
                                               public MapObserver
{
    Q_OBJECT
public:
    explicit TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent = nullptr);

    /// Live handler while GameClient is up (ProtocolGame tile-description apply).
    static TWorldmapProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TWorldmapProtocolMessageHandler* handler);

    TWorldMapStorage* storage() const { return m_storage; }

    /// CIP tile-description apply (binary decode stays in ProtocolGame).
    void clearField(const Position& position);
    void applyAppearanceToField(const appearances::TAppearanceInstancePtr& appearance, const Position& position,
                                int stackPos);
    void applyCreatureIdToField(uint32_t creatureId, const Position& position, int stackPos);

    /// CIP slot: AmbientLight (S 0x82 / 130) — native Q→H apply (skips classic bridge).
    void handleAmbientLightMessage(
        const ::tibia::protobuf::protocol::GameserverMessageAmbientLight& AmbientLightMessage);

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);

    /// MapObserver — CIP handleCreate/Change/DeleteOnMap → storage coordinate.
    void onTileChanged(const Position& position) override;
    /// MapObserver — CIP handleFullMap / floor/row/column → storage volume.
    void onWindowChanged(bool fullDescription) override;

    /// Aliases kept for call sites that still use protocol-handler names.
    void onMapWindowChanged(bool fullDescription) { onWindowChanged(fullDescription); }

Q_SIGNALS:
    void publishGameAction();

private:
    TWorldMapStorage* m_storage = nullptr;
};

}
}
