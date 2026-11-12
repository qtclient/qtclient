#include "minimapmarkerstorage.h"

#include "input/gameactionhandlers.h"
#include "minimap/minimapmanager.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "sprites/sprite.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <any>
#include <vector>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::minimap {

namespace {

Q_LOGGING_CATEGORY(lcMarkers, "tibia.minimap.markers")

using A = input::EGameAction;

constexpr A HandledActions[] = {
    A::SHOW_DIALOG_CREATE_MINIMAP_MARKER,
    A::SHOW_DIALOG_EDIT_MINIMAP_MARKER,
    A::UPDATE_MINIMAP_MARKER,
    A::DELETE_MINIMAP_MARKER,
    A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION,
};

QString keyOf(const Position& pos)
{
    return QStringLiteral("%1,%2,%3").arg(pos.x).arg(pos.y).arg(pos.z);
}

TMinimapMarkerStorage* g_activeMarkers = nullptr;

}

TMinimapMarkerStorage* TMinimapMarkerStorage::active() { return g_activeMarkers; }

void TMinimapMarkerStorage::setActive(TMinimapMarkerStorage* storage) { g_activeMarkers = storage; }

TMinimapMarkerStorage::TMinimapMarkerStorage(const QString& bundledMarkersJson, QObject* parent)
    : cip::tibia::minimap::TMinimapMarkerStorage(parent)
    , m_bundledMarkersJson(bundledMarkersJson)
    , m_dialog(std::make_unique<gamewindow::TEditMinimapMarkerDialogController>(this))
    , m_markers(MINIMAP_MARKER_FLOORS)
{
    using Args = std::vector<std::any>;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onAddAutomapFlag", [this](GameObject*, const Args& args) {
        const auto* pos = args.empty() ? nullptr : std::any_cast<Position>(&args[0]);
        if (!pos)
            return std::any{};
        const auto icon = static_cast<uint8_t>(client::gamecore::argInt(args, 1));
        createOrUpdate(*pos, icon, client::gamecore::argString(args, 2));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRemoveAutomapFlag", [this](GameObject*, const Args& args) {
        const auto* pos = args.empty() ? nullptr : std::any_cast<Position>(&args[0]);
        if (pos)
            remove(*pos);
        return std::any{};
    });
    bindActions();
    load();
}

TMinimapMarkerStorage::~TMinimapMarkerStorage()
{
    if (g_activeMarkers == this)
        g_activeMarkers = nullptr;
    if (m_savePending)
        saveUserMarkers();
    unbindActions();
}

QString TMinimapMarkerStorage::userMarkersPath()
{
    return TMinimapManager::minimapDir() + QStringLiteral("/user-markers.json");
}

void TMinimapMarkerStorage::clearMarkers()
{
    for (auto& floor : m_markers)
        floor.clear();
}

bool TMinimapMarkerStorage::addMarker(const Position& pos, const uint8_t icon, const std::string& description)
{
    if (!pos.isValid() || pos.x < 0 || pos.x > UINT16_MAX || pos.y < 0 || pos.y > UINT16_MAX
        || pos.z >= static_cast<int>(m_markers.size()) || icon >= MINIMAP_MARKER_ICONS)
        return false;

    m_markers[pos.z].push_back({ static_cast<uint16_t>(pos.x), static_cast<uint16_t>(pos.y), icon, description });
    return true;
}

bool TMinimapMarkerStorage::removeMarker(const Position& pos)
{
    if (!pos.isValid() || pos.z >= static_cast<int>(m_markers.size()))
        return false;
    auto& floor = m_markers[pos.z];
    const auto it = std::find_if(floor.begin(), floor.end(), [&](const MinimapMarker& marker) {
        return marker.x == pos.x && marker.y == pos.y;
    });
    if (it == floor.end())
        return false;
    floor.erase(it);
    return true;
}

bool TMinimapMarkerStorage::updateMarker(const Position& pos, const uint8_t icon, const std::string& description)
{
    if (!pos.isValid() || pos.z >= static_cast<int>(m_markers.size()) || icon >= MINIMAP_MARKER_ICONS)
        return false;
    auto& floor = m_markers[pos.z];
    for (MinimapMarker& marker : floor) {
        if (marker.x == pos.x && marker.y == pos.y) {
            marker.icon = icon;
            marker.description = description;
            return true;
        }
    }
    return addMarker(pos, icon, description);
}

bool TMinimapMarkerStorage::findMarker(const Position& pos, MinimapMarker& out) const
{
    if (!pos.isValid() || pos.z >= static_cast<int>(m_markers.size()))
        return false;
    for (const MinimapMarker& marker : m_markers[pos.z]) {
        if (marker.x == pos.x && marker.y == pos.y) {
            out = marker;
            return true;
        }
    }
    return false;
}

const std::vector<MinimapMarker>& TMinimapMarkerStorage::floorMarkers(const uint8_t z) const
{
    static const std::vector<MinimapMarker> empty;
    return z < m_markers.size() ? m_markers[z] : empty;
}

void TMinimapMarkerStorage::load()
{
    clearMarkers();
    m_userOwned.clear();
    m_deleted.clear();
    if (!m_bundledMarkersJson.isEmpty())
        loadJsonFile(m_bundledMarkersJson, /*userOwned=*/false);
    loadJsonFile(userMarkersPath(), /*userOwned=*/true);
    Q_EMIT markersChanged();
    qmlcomponents::TMinimapRendererQuickItem::notifyMarkersChanged();
}

void TMinimapMarkerStorage::loadJsonFile(const QString& path, const bool userOwned)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (userOwned)
            qCDebug(lcMarkers) << "no user markers at" << path;
        else
            qCInfo(lcMarkers) << "bundled markers missing:" << path;
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    const QJsonArray markers = doc.isObject() ? root.value(QStringLiteral("markers")).toArray()
                                              : (doc.isArray() ? doc.array() : QJsonArray{});
    size_t loaded = 0;
    for (const QJsonValue& value : markers) {
        const QJsonArray row = value.toArray();
        if (row.size() < 4)
            continue;
        const Position pos(row.at(0).toInt(), row.at(1).toInt(), static_cast<uint8_t>(row.at(2).toInt()));
        const auto icon = static_cast<uint8_t>(row.at(3).toInt());
        const QString description = row.size() >= 5 ? row.at(4).toString() : QString();
        if (userOwned) {
            if (updateMarker(pos, icon, description.toStdString())) {
                m_userOwned.insert(keyOf(pos));
                ++loaded;
            }
        } else if (addMarker(pos, icon, description.toStdString())) {
            ++loaded;
        }
    }
    if (userOwned) {
        for (const QJsonValue& value : root.value(QStringLiteral("deleted")).toArray()) {
            const QJsonArray row = value.toArray();
            if (row.size() < 3)
                continue;
            const Position pos(row.at(0).toInt(), row.at(1).toInt(), static_cast<uint8_t>(row.at(2).toInt()));
            removeMarker(pos);
            m_deleted.insert(keyOf(pos));
            m_userOwned.remove(keyOf(pos));
        }
    }
    qCInfo(lcMarkers) << "loaded" << loaded << "markers from" << path;
}

void TMinimapMarkerStorage::saveUserMarkers() const
{
    QDir().mkpath(TMinimapManager::minimapDir());
    QJsonArray markers;
    for (const QString& key : m_userOwned) {
        const QStringList parts = key.split(QLatin1Char(','));
        if (parts.size() != 3)
            continue;
        const Position pos(parts[0].toInt(), parts[1].toInt(), static_cast<uint8_t>(parts[2].toInt()));
        MinimapMarker marker;
        if (!findMarker(pos, marker))
            continue;
        markers.append(QJsonArray{ marker.x, marker.y, pos.z, marker.icon,
                                   QString::fromStdString(marker.description) });
    }
    QJsonArray deleted;
    for (const QString& key : m_deleted) {
        const QStringList parts = key.split(QLatin1Char(','));
        if (parts.size() != 3)
            continue;
        deleted.append(QJsonArray{ parts[0].toInt(), parts[1].toInt(), parts[2].toInt() });
    }
    QJsonObject root;
    root.insert(QStringLiteral("markers"), markers);
    root.insert(QStringLiteral("deleted"), deleted);
    const QString path = userMarkersPath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcMarkers) << "failed to write" << path << file.errorString();
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    qCDebug(lcMarkers) << "saved" << markers.size() << "user markers to" << path;
}

void TMinimapMarkerStorage::scheduleSave()
{
    m_savePending = true;
    Q_EMIT requestDelayedCallback();
    QTimer::singleShot(0, this, &TMinimapMarkerStorage::onDelayedCallback);
}

void TMinimapMarkerStorage::onDelayedCallback()
{
    if (!m_savePending)
        return;
    m_savePending = false;
    saveUserMarkers();
}

std::optional<MinimapMarker> TMinimapMarkerStorage::markerAt(const Position& pos) const
{
    MinimapMarker marker;
    if (!findMarker(pos, marker))
        return std::nullopt;
    return marker;
}

bool TMinimapMarkerStorage::createOrUpdate(const Position& pos, const uint8_t icon, const QString& description)
{
    // CIP sprites::TStaticMinimapMarkerSymbolResourceSpriteIdentifier — icon resource path.
    const std::string symbolPath = "qrc:/images/game/minimap/marker" + std::to_string(static_cast<int>(icon)) + ".png";
    (void)sprites::TStaticMinimapMarkerSymbolResourceSpriteIdentifier(symbolPath);
    if (!updateMarker(pos, icon, description.toStdString()))
        return false;
    m_userOwned.insert(keyOf(pos));
    m_deleted.remove(keyOf(pos));
    scheduleSave();
    Q_EMIT markersChanged();
    qmlcomponents::TMinimapRendererQuickItem::notifyMarkersChanged();
    return true;
}

bool TMinimapMarkerStorage::remove(const Position& pos)
{
    if (!removeMarker(pos))
        return false;
    m_userOwned.remove(keyOf(pos));
    m_deleted.insert(keyOf(pos));
    scheduleSave();
    Q_EMIT markersChanged();
    qmlcomponents::TMinimapRendererQuickItem::notifyMarkersChanged();
    return true;
}

void TMinimapMarkerStorage::bindActions()
{
    input::setActionHandler(A::SHOW_DIALOG_CREATE_MINIMAP_MARKER, [this](const input::TGameAction& action) {
        m_pendingPosition = action.position;
        m_dialog->open(action.position, {}, 0, false);
        return true;
    });
    input::setActionHandler(A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION, [this](const input::TGameAction&) {
        if (!tibia::game::localPlayer())
            return false;
        const Position pos = tibia::game::localPlayerPosition();
        if (!pos.isValid())
            return false;
        m_pendingPosition = pos;
        m_dialog->open(pos, {}, 0, false);
        return true;
    });
    input::setActionHandler(A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, [this](const input::TGameAction& action) {
        m_pendingPosition = action.position;
        const auto existing = markerAt(action.position);
        m_dialog->open(action.position,
                       existing ? QString::fromStdString(existing->description) : QString(),
                       existing ? existing->icon : 0, true);
        return true;
    });
    input::setActionHandler(A::UPDATE_MINIMAP_MARKER, [this](const input::TGameAction& action) {
        const Position pos = action.position.isValid() ? action.position : m_pendingPosition;
        const auto icon = static_cast<uint8_t>(std::clamp(action.parameter, 0, static_cast<int>(MINIMAP_MARKER_ICONS - 1)));
        return createOrUpdate(pos, icon, action.text);
    });
    input::setActionHandler(A::DELETE_MINIMAP_MARKER, [this](const input::TGameAction& action) {
        return remove(action.position);
    });
}

void TMinimapMarkerStorage::unbindActions()
{
    for (const A type : HandledActions)
        input::setActionHandler(type, nullptr);
}

}
