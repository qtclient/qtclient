#pragma once

#include <cip_gen.h>

#include <client/position.h>

namespace tibia::worldmap {

class TWorldMapStorage;

/// CIP name for map/ambient protocol ingress (TWorldmapProtocolMessageHandler).
/// Binary decode stays in ProtocolGame::parseMap* / parseWorldTime; this object
/// bridges MapObserver-style window/tile notifications into TWorldMapStorage
/// (minimap feed comment at TMinimapWorldFeed::onWindowChanged).
class TWorldmapProtocolMessageHandler final : public cip::tibia::worldmap::TWorldmapProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent = nullptr);

    TWorldMapStorage* storage() const { return m_storage; }

    /// After ProtocolGame finishes a map description / step (CIP handleFullMap /
    /// row/column/floor handlers → storage volume).
    void onMapWindowChanged(bool fullDescription);
    /// After a tile's things change (CIP handleCreate/Change/DeleteOnMap).
    void onTileChanged(const Position& position);

Q_SIGNALS:
    void publishGameAction();

private:
    TWorldMapStorage* m_storage = nullptr;
};

}
