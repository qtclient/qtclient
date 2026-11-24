#include "minimapmarkerstorage.h"

#include "input/gameactionhandlers.h"
#include "qmlcomponents/minimaprendererquickitem.h"

#include <algorithm>
#include <any>
#include <vector>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::minimap {

namespace {

using A = input::EGameAction;
using worldmap::TWorldMapCoordinate;

// The actions the storage handles. // not-a-cip-table
constexpr A HandledActions[] = {
    A::SHOW_DIALOG_CREATE_MINIMAP_MARKER,
    A::SHOW_DIALOG_EDIT_MINIMAP_MARKER,
    A::UPDATE_MINIMAP_MARKER,
    A::DELETE_MINIMAP_MARKER,
    A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION,
};

TMinimapMarkerStorage* g_activeMarkers = nullptr;

}

TMinimapMarkerStorage* TMinimapMarkerStorage::active() { return g_activeMarkers; }

void TMinimapMarkerStorage::setActive(TMinimapMarkerStorage* storage) { g_activeMarkers = storage; }

TMinimapMarkerStorage::TMinimapMarkerStorage(QObject* parent)
    : cip::tibia::minimap::TMinimapMarkerStorage(parent)
    , m_dialog(std::make_unique<gamewindow::TEditMinimapMarkerDialogController>(this, nullptr, nullptr, this))
{
    using Args = std::vector<std::any>;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onAddAutomapFlag", [this](GameObject*, const Args& args) {
        const auto* pos = args.empty() ? nullptr : std::any_cast<Position>(&args[0]);
        if (!pos)
            return std::any{};
        const auto icon = static_cast<uint8_t>(client::gamecore::argInt(args, 1));
        addMarker(TMinimapMarker(TWorldMapCoordinate(*pos), icon,
                                 client::gamecore::argString(args, 2),
                                 QDateTime::currentDateTimeUtc()));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRemoveAutomapFlag", [this](GameObject*, const Args& args) {
        if (const auto* pos = args.empty() ? nullptr : std::any_cast<Position>(&args[0]))
            removeMarker(TWorldMapCoordinate(*pos));
        return std::any{};
    });
    bindActions();
}

TMinimapMarkerStorage::~TMinimapMarkerStorage()
{
    if (g_activeMarkers == this)
        g_activeMarkers = nullptr;
    unbindActions();
}

TMinimapMarkerPtr TMinimapMarkerStorage::markerAt(const TWorldMapCoordinate& coordinate) const
{
    const auto it = m_markers.find(coordinate);
    return it != m_markers.end() ? it->second : nullptr;
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
    // The marker dialog's actions.
    for (const A type : { A::SHOW_DIALOG_CREATE_MINIMAP_MARKER, A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, // not-a-cip-table
                          A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION }) {
        m_gameActions.on(type, [this](const input::IGameAction& action) {
            m_dialog->handleGameAction(action);
            return true;
        });
    }
    m_gameActions.on(A::UPDATE_MINIMAP_MARKER, [this](const input::TGameActionUpdateMinimapMarker& action) {
        const input::TMinimapMarkerData& marker = action.marker();
        const auto icon = static_cast<uint8_t>(std::clamp(int(marker.icon), 0, static_cast<int>(MINIMAP_MARKER_ICONS - 1)));
        addMarker(TMinimapMarker(TWorldMapCoordinate(marker.coordinate), icon, marker.description, QDateTime::currentDateTimeUtc()));
        return true;
    });
    m_gameActions.on(A::DELETE_MINIMAP_MARKER, [this](const input::TGameActionDeleteMinimapMarker& action) {
        removeMarker(TWorldMapCoordinate(action.coordinate()));
        return true;
    });
}

void TMinimapMarkerStorage::unbindActions()
{
}

}
