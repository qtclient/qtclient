#include "minimapmarkerstorage.h"

#include "input/gameactionhandlers.h"
#include "qmlcomponents/minimaprendererquickitem.h"

#include <algorithm>
#include <any>
#include <vector>
#include <tibia/game/gamenotify.h>
#include "container/containerciptypes.h"
#include <functional>

namespace tibia::minimap {

namespace {

using A = input::EGameAction;
using worldmap::TWorldMapCoordinate;

// The actions the storage handles (DELETE_MINIMAP_MARKER is the marker game
// action handler's). // not-a-cip-table
constexpr A HandledActions[] = {
    A::UPDATE_MINIMAP_MARKER,
};

}

TMinimapMarkerStorage::TMinimapMarkerStorage(QObject* parent)
    : cip::tibia::minimap::TMinimapMarkerStorage(parent)
{
    bindActions();
}

TMinimapMarkerStorage::~TMinimapMarkerStorage()
{
    unbindActions();
}

TMinimapMarkerPtr TMinimapMarkerStorage::markerAt(const TWorldMapCoordinate& coordinate) const
{
    const auto it = m_markers.find(coordinate);
    return it != m_markers.end() ? it->second : nullptr;
}

input::TMinimapMarkerData TMinimapMarkerStorage::markerData(const Position& coord) const
{
    input::TMinimapMarkerData data{ coord, 0, {} };
    if (const TMinimapMarkerPtr marker = markerAt(TWorldMapCoordinate(coord))) {
        data.icon = marker->icon;
        data.description = marker->description;
    }
    return data;
}

void TMinimapMarkerStorage::addMarker(const TMinimapMarker& marker)
{
    m_markers[marker.coordinate] = std::make_shared<TMinimapMarker>(marker);
    changed();
}

void TMinimapMarkerStorage::removeMarker(const TWorldMapCoordinate& coordinate)
{
    m_markers.erase(coordinate);
    changed();
}

void TMinimapMarkerStorage::changed()
{
    m_dirty = true;
    Q_EMIT requestDelayedCallback();
    Q_EMIT markersChanged();
    qmlcomponents::TMinimapRendererQuickItem::notifyMarkersChanged();
}

std::vector<TMinimapMarkerPtr> TMinimapMarkerStorage::markers(
    const std::function<bool(const TWorldMapCoordinate&)>& accept) const
{
    std::vector<TMinimapMarkerPtr> result;
    for (const auto& [coordinate, marker] : m_markers) {
        if (accept(coordinate))
            result.push_back(marker);
    }
    return result;
}

std::vector<TMinimapMarkerPtr> TMinimapMarkerStorage::markers() const
{
    std::vector<TMinimapMarkerPtr> result;
    result.reserve(m_markers.size());
    for (const auto& [coordinate, marker] : m_markers)
        result.push_back(marker);
    return result;
}

void TMinimapMarkerStorage::saveMarkers()
{
    std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> content = fileContentFromMarkers(m_markers);
    m_dirty = false;
    Q_EMIT startSavingMinimapMarkerFileContentToDisk(content);
}

void TMinimapMarkerStorage::onDelayedCallback()
{
    if (m_dirty)
        saveMarkers();
}

void TMinimapMarkerStorage::setMarkersFromMinimapMarkerFile(const tibia::protobuf::minimap::MinimapMarkerFileContent& fileContent)
{
    m_markers.clear();
    for (auto& [coordinate, marker] : markersFromFileContent(fileContent)) {
        if (!isObsoleteMarker(*marker))
            m_markers[coordinate] = std::move(marker);
    }
    Q_EMIT markersChanged();
    qmlcomponents::TMinimapRendererQuickItem::notifyMarkersChanged();
}

void TMinimapMarkerStorage::bindActions()
{
    m_gameActions.on(A::UPDATE_MINIMAP_MARKER, [this](const input::TGameActionUpdateMinimapMarker& action) {
        const input::TMinimapMarkerData& marker = action.marker();
        const auto icon = static_cast<uint8_t>(std::clamp(int(marker.icon), 0, static_cast<int>(MINIMAP_MARKER_ICONS - 1)));
        addMarker(TMinimapMarker(TWorldMapCoordinate(marker.coordinate), icon, marker.description, QDateTime::currentDateTimeUtc()));
        return true;
    });
}

void TMinimapMarkerStorage::unbindActions()
{
}

}
