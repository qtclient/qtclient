#pragma once

#include "worldmap/coordinates.h"

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <map>
#include <memory>

namespace tibia::protobuf::minimap {
class MinimapMarkerFileContent;
}

namespace tibia {
namespace minimap {

/// The minimap flag icons (images/game/minimap/flag<icon>).
constexpr uint8_t MINIMAP_MARKER_ICONS = 20;

/// A minimap flag: where it is, its icon and text, and when it was set.
struct TMinimapMarker
{
    TMinimapMarker() = default;
    TMinimapMarker(const worldmap::TWorldMapCoordinate& coordinate_, const uint8_t icon_, QString description_,
                   QDateTime creationTime_ = {})
        : coordinate(coordinate_)
        , icon(icon_)
        , description(std::move(description_))
        , creationTime(std::move(creationTime_))
    {
    }

    worldmap::TWorldMapCoordinate coordinate;
    uint8_t icon = 0;
    QString description;
    QDateTime creationTime;
};

using TMinimapMarkerPtr = std::shared_ptr<TMinimapMarker>;

/// The marker file content of `markers`.
std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent>
fileContentFromMarkers(const std::map<worldmap::TWorldMapCoordinate, TMinimapMarkerPtr>& markers);
/// The markers of a marker file, by coordinate.
std::map<worldmap::TWorldMapCoordinate, TMinimapMarkerPtr>
markersFromFileContent(const tibia::protobuf::minimap::MinimapMarkerFileContent& content);
/// One of the obsolete spell markers older marker files carry; they are dropped on load.
bool isObsoleteMarker(const TMinimapMarker& marker);

}
}
