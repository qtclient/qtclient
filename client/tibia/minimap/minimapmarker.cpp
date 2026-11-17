#include "minimapmarker.h"

#include <minimap.pb.h>

#include <algorithm>
#include <array>

namespace tibia::minimap {

using worldmap::TWorldMapCoordinate;

namespace {

struct TObsoleteMarker
{
    int x, y, z;
    uint8_t icon;
    const char* description;
};

/// The spell teacher flags of older marker files.
constexpr std::array<TObsoleteMarker, 16> ObsoleteMarkers{ {
    { 32302, 31816, 7, 3, "Druid Spells" },
    { 33239, 32404, 7, 3, "Paladin and Knight Spells" },
    { 33221, 32410, 7, 3, "Sorcerer and Druid Spells" },
    { 32269, 32842, 7, 3, "Druid Spells" },
    { 32414, 32831, 4, 3, "Paladin Spells" },
    { 33148, 32868, 6, 3, "Sorcerer Spells" },
    { 33160, 32812, 5, 3, "Knight Spells" },
    { 33611, 31888, 6, 19, "Knight Spells" },
    { 33631, 31880, 4, 19, "Sorcerer Spells" },
    { 32406, 32202, 6, 3, "Knight Spells" },
    { 32438, 32213, 8, 3, "Druid Spells" },
    { 32931, 32074, 11, 3, "Knight and Paladin Spells" },
    { 32905, 31020, 7, 3, "Sorcerer Spells" },
    { 32894, 31059, 7, 3, "Druid Spells" },
    { 32879, 31077, 7, 3, "Paladin Spells" },
    { 32721, 31288, 7, 3, "Knight Spells" },
} };

}

std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent>
fileContentFromMarkers(const std::map<TWorldMapCoordinate, TMinimapMarkerPtr>& markers)
{
    auto content = std::make_shared<tibia::protobuf::minimap::MinimapMarkerFileContent>();
    for (const auto& [coordinate, marker] : markers) {
        tibia::protobuf::minimap::MinimapMarker* entry = content->add_marker();
        tibia::protobuf::shared::Coordinate* position = entry->mutable_position();
        position->set_x(static_cast<uint32_t>(marker->coordinate.x));
        position->set_y(static_cast<uint32_t>(marker->coordinate.y));
        position->set_z(static_cast<uint32_t>(marker->coordinate.z));
        entry->set_icon(marker->icon);
        entry->set_description(marker->description.toStdString());
        entry->set_timestamp(marker->creationTime.toMSecsSinceEpoch());
    }
    return content;
}

std::map<TWorldMapCoordinate, TMinimapMarkerPtr>
markersFromFileContent(const tibia::protobuf::minimap::MinimapMarkerFileContent& content)
{
    std::map<TWorldMapCoordinate, TMinimapMarkerPtr> markers;
    for (const tibia::protobuf::minimap::MinimapMarker& entry : content.marker()) {
        const TWorldMapCoordinate coordinate(static_cast<int>(entry.position().x()), static_cast<int>(entry.position().y()),
                                             static_cast<int>(entry.position().z()));
        markers[coordinate] = std::make_shared<TMinimapMarker>(
            coordinate, static_cast<uint8_t>(entry.icon()),
            QString::fromUtf8(entry.description().data(), static_cast<qsizetype>(entry.description().size())),
            QDateTime::fromMSecsSinceEpoch(entry.timestamp()));
    }
    return markers;
}

bool isObsoleteMarker(const TMinimapMarker& marker)
{
    return std::ranges::any_of(ObsoleteMarkers, [&](const TObsoleteMarker& obsolete) {
        return obsolete.icon == marker.icon && marker.coordinate == TWorldMapCoordinate(obsolete.x, obsolete.y, obsolete.z)
               && marker.description == QLatin1StringView(obsolete.description);
    });
}

}
