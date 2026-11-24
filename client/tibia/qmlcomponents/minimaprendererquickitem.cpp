#include "minimaprendererquickitem.h"
#include "cyclopedia/staticcontentinformation.h"

#include "minimap/minimaprendererscenegraphdrawcommandprovider.h"
#include "minimap/minimaprenderinfostorage.h"
#include "qmlcomponents/qsgscenegraphdrawcommandsnode.h"
#include "qmlcomponents/worldmapqmltypes.h"
#include "renderer/scenegraphnodeprovider.h"
#include "renderer/worldmapcamera.h"
#include "sprites/resourcespritesmanager.h"

#include "client/gameclient.h"
#include "game/tgamenotify.h"
#include "cyclopedia/cyclopediamapdata.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "gamewindow/tooltiptextvisitor.h"
#include "sprites/spritesheetdecompressor.h"
#include "sprites/spritesystem.h"
#include "minimap/minimapmarkerstorage.h"
#include "minimap/minimaptile.h"
#include "minimap/minimaptilestorage.h"
#include "renderer/spritetextureprovider.h"

#include <tibia/assets/things.h>
#include "clockservice.h"
#include "shared/tibiafilesystemhelper.h"
#include "assets/cipbmpimage.h"

#include <QList>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QQmlComponent>
#include <QThread>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGOpacityNode>
#include <QSGRectangleNode>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"

namespace tibia::qmlcomponents {

namespace {

using EFileType = cyclopedia::TCyclopediaMapStaticDataStorage::EMapFileType;
using TStaticData = cyclopedia::TCyclopediaMapStaticDataStorage;
using ETileType = cip::tibia::qmlcomponents::TMinimapRendererQuickItem::EMinimapTileType;

constexpr int SubfieldsPerField = 32;
constexpr int GroundLayer = 7;
constexpr int MaxLayer = 15;

minimap::TMinimapTileStorage* g_minimapTiles = nullptr;
QList<TMinimapRendererQuickItem*> g_minimapRenderers;

/// The time the draw commands wait after a zoom before they are rebuilt.
constexpr int RescalingTimerInterval = 500;

void deleteTextures(const std::unordered_map<qint64, QSGTexture*>& textures)
{
    for (const auto& [key, texture] : textures)
        delete texture;
}

/// Host: the item's scene graph: the draw commands' node, then the pack's
/// mission zone images (with the textures drawn last frame).
class TRendererNode final : public QSGNode
{
public:
    explicit TRendererNode(TQSGScenegraphDrawCommandsNode* commands)
        : drawCommands(commands)
    {
        appendChildNode(commands);
        appendChildNode(missionZones);
    }
    ~TRendererNode() override { deleteTextures(textures); }
    void clearMissionZones()
    {
        while (QSGNode* child = missionZones->firstChild()) {
            missionZones->removeChildNode(child);
            delete child;
        }
    }
    TQSGScenegraphDrawCommandsNode* drawCommands;
    QSGNode* missionZones = new QSGNode;
    std::unordered_map<qint64, QSGTexture*> textures;
};

}

TWorldMapCamera::TWorldMapCamera(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCameraQmlType(parent)
    , m_camera(new renderer::TWorldMapCamera(this))
{
    auto* topLeft = new TWorldMapSubfieldCoordinateQmlType(this);
    m_topLeftSubfieldCoordinate = topLeft;
    connect(topLeft, &cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType::coordinateChanged, this,
            &TWorldMapCamera::onTopLeftCoordinateChanged);
    connect(m_camera, &renderer::TWorldMapCamera::cameraViewportChanged, this, [this] {
        syncTopLeftSubfieldCoordinate();
        Q_EMIT cameraViewportChanged();
    });
    // The map opens on the ground layer at 1/16 until its QML sets the scale.
    m_camera->setTopLeftSubfieldCoordinate(worldmap::TWorldMapSubfieldCoordinate(0, 0, GroundLayer));
    m_camera->setScaleFactor(1.0f / 16.0f);
}

QPoint TWorldMapCamera::topLeft() const
{
    const worldmap::TWorldMapSubfieldCoordinate topLeft = m_camera->topLeftSubfieldCoordinate();
    return { topLeft.subfieldX, topLeft.subfieldY };
}

int TWorldMapCamera::layer() const { return m_camera->topLeftSubfieldCoordinate().z; }

void TWorldMapCamera::setViewportSize(const QSizeF& size) { m_camera->setViewportSize(size.toSize()); }

QPointF TWorldMapCamera::pixelOf(const int x, const int y, const int layer) const
{
    const QPoint origin = topLeft() + QPoint(this->layer(), this->layer()) * SubfieldsPerField;
    return QPointF((x + layer) * SubfieldsPerField - origin.x(), (y + layer) * SubfieldsPerField - origin.y()) * m_camera->scaleFactor();
}

QPoint TWorldMapCamera::fieldAt(const QPointF& point) const
{
    const worldmap::TWorldMapCoordinate field = m_camera->convertToWorldMapSubfieldCoordinate(point.toPoint()).toFieldCoordinate();
    return { field.x, field.y };
}

void TWorldMapCamera::syncTopLeftSubfieldCoordinate()
{
    const worldmap::TWorldMapSubfieldCoordinate topLeft = m_camera->topLeftSubfieldCoordinate();
    auto* coordinate = m_topLeftSubfieldCoordinate;
    if (coordinate->subfieldX() == topLeft.subfieldX && coordinate->subfieldY() == topLeft.subfieldY && coordinate->z() == topLeft.z)
        return;
    {
        // One change notification for all three components.
        const QSignalBlocker blocker(coordinate);
        coordinate->setSubfieldX(topLeft.subfieldX);
        coordinate->setSubfieldY(topLeft.subfieldY);
        coordinate->setZ(topLeft.z);
    }
    Q_EMIT coordinate->coordinateChanged();
}

void TWorldMapCamera::onTopLeftCoordinateChanged()
{
    const worldmap::TWorldMapSubfieldCoordinate coordinate(m_topLeftSubfieldCoordinate->subfieldX(), m_topLeftSubfieldCoordinate->subfieldY(),
                                                           m_topLeftSubfieldCoordinate->z());
    if (m_camera->topLeftSubfieldCoordinate() == coordinate)
        return;
    m_camera->setTopLeftSubfieldCoordinate(coordinate);
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::setTopLeftSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v)
{
    if (!v)
        return;
    auto* coordinate = m_topLeftSubfieldCoordinate;
    {
        const QSignalBlocker blocker(coordinate);
        coordinate->setSubfieldX(v->subfieldX());
        coordinate->setSubfieldY(v->subfieldY());
        coordinate->setZ(v->z());
    }
    Q_EMIT coordinate->coordinateChanged();
}

float TWorldMapCamera::scaleFactor() const { return m_camera->scaleFactor(); }

void TWorldMapCamera::setScaleFactor(const float v) { m_camera->setScaleFactor(v); }

QPoint TWorldMapCamera::referencePoint() const { return m_camera->referencePoint(); }

void TWorldMapCamera::setReferencePoint(const QPoint& v) { m_camera->setReferencePoint(v); }

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TWorldMapCamera::coordinateAtPoint(const QPoint& Value)
{
    return TWorldMapCoordinateQmlType::create(m_camera->convertToWorldMapSubfieldCoordinate(Value).toFieldCoordinate());
}

void TWorldMapCamera::changeLayerWithIsometricProjection(int NewLayer)
{
    NewLayer = std::clamp(NewLayer, 0, MaxLayer);
    const worldmap::TWorldMapSubfieldCoordinate topLeft = m_camera->topLeftSubfieldCoordinate();
    // The same place seen from the new layer: one field down-right per layer up.
    worldmap::TWorldMapSubfieldCoordinate projected = worldmap::subfieldOfField(
        topLeft.toFieldCoordinate().shiftedAwayFromFloor(NewLayer), worldmap::TWorldMapSubfieldExtent::offsetInField(topLeft));
    projected.z = NewLayer;
    m_camera->setTopLeftSubfieldCoordinate(projected);
    Q_EMIT m_camera->cameraViewportChanged();
}

void TWorldMapCamera::moveDistanceRelativeToReferencePoint(const QPoint& Distance) { m_camera->moveDistanceRelativeToReferencePoint(Distance); }

void TWorldMapCamera::centerAtSubfieldCoordinateDelayed(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pSubfieldCoordinate)
{
    if (pSubfieldCoordinate)
        m_camera->centerAtSubfieldCoordinateDelayed(
            worldmap::TWorldMapSubfieldCoordinate(pSubfieldCoordinate->subfieldX(), pSubfieldCoordinate->subfieldY(), pSubfieldCoordinate->z()));
}

void TWorldMapCamera::centerDelayed(const int subfieldX, const int subfieldY, const int layer)
{
    m_pendingCenter = worldmap::TWorldMapSubfieldCoordinate(subfieldX, subfieldY, layer);
    m_hasPendingCenter = true;
    // Polishes the renderer, which applies it.
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::setTopLeftDelayed(const int subfieldX, const int subfieldY, const int layer)
{
    m_camera->setTopLeftSubfieldCoordinateDelayed(worldmap::TWorldMapSubfieldCoordinate(subfieldX, subfieldY, layer));
}

void TWorldMapCamera::applyPendingCenter()
{
    if (!m_hasPendingCenter || m_camera->viewportSize().isEmpty())
        return;
    m_hasPendingCenter = false;
    m_camera->centerAtSubfieldCoordinate(m_pendingCenter);
}

TAreaCaptionController::TAreaCaptionController(const int id, const bool isArea, QObject* parent)
    : cip::tibia::qmlcomponents::TAreaCaptionController(parent)
    , m_id(id)
    , m_isArea(isArea)
{
}

void TAreaCaptionController::set(const QString& caption, const QColor& color, const bool improvedRespawnActive)
{
    const QString colorName = color.name();
    if (m_caption == caption && m_captionColor == colorName && m_isImprovedRespawnActive == improvedRespawnActive)
        return;
    m_caption = caption;
    m_captionColor = colorName;
    m_isImprovedRespawnActive = improvedRespawnActive;
    Q_EMIT propertyChanged();
}

void TAreaCaptionController::onClicked() { Q_EMIT clicked(m_id, m_isArea); }

void TAreaCaptionController::activate() { onClicked(); }

TMinimapRendererQuickItem::TMinimapRendererQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TMinimapRendererQuickItem(parent)
    , m_worldCamera(new TWorldMapCamera(this))
    , m_renderInfoStorage(std::make_shared<minimap::TMinimapRenderInfoStorage>())
    , m_resourceSpritesManager(std::make_shared<sprites::TResourceSpritesManager>())
{
    g_minimapRenderers.append(this);
    setFlag(ItemHasContents, true);
    m_camera = m_worldCamera;
    m_showAreaCaptions = true;
    m_showZones = true;
    m_showExploreOverlay = true;
    m_showNPCMarkers = true;
    m_showPassages = true;
    m_showHouses = true;
    m_minimapMarkersFilter = QList<bool>(40, true);
    // Host: the render infos read the process' static map data, and the item
    // owns the resource images the markers are drawn with.
    m_renderInfoStorage->setCamera(m_worldCamera->camera());
    m_renderInfoStorage->setStaticDataStorage(&TStaticData::instance());
    m_renderInfoStorage->setResourceSpritesManager(m_resourceSpritesManager.get());
    m_drawCommandProvider = std::make_shared<minimap::TMinimapRendererScenegraphDrawCommandProvider>(m_renderInfoStorage);
    m_drawCommandProvider->setResourceSpritesManager(m_resourceSpritesManager.get());
    m_rescalingTimer.setSingleShot(true);
    m_rescalingTimer.setInterval(RescalingTimerInterval);
    connect(&m_rescalingTimer, &QTimer::timeout, this, &TMinimapRendererQuickItem::onRescalingTimerTimeout);
    connect(m_worldCamera, &TWorldMapCamera::cameraViewportChanged, this, &TMinimapRendererQuickItem::onCameraViewportChanged);
    connect(m_renderInfoStorage.get(), &minimap::TMinimapRenderInfoStorage::renderInfosChanged, this,
            &TMinimapRendererQuickItem::onRenderInfosChanged);
    updateRenderInfoSettings();
    ensureMarkerIcons();
    connect(this, &TMinimapRendererQuickItem::propertyChanged, this, [this] {
        updateRenderInfoSettings();
        scheduleUpdate();
    });
    // Map images come from the sprite system; redraw when one is decompressed.
    connect(tibia::sprites::TSpriteSystem::instance().decompressor().get(),
            &tibia::sprites::TSpritesheetDecompressor::decompressingSpritesheetFinished, this,
            &TMinimapRendererQuickItem::onRenderInfosChanged);
    // Host: the player marker follows the local player.
    m_playerTimer.setInterval(250);
    connect(&m_playerTimer, &QTimer::timeout, this, [this] {
        const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
        if (!session || !session->hasLocalPlayer())
            return;
        const Position position = session->localPlayerPosition();
        if (QPoint(position.x, position.y) == m_playerField && position.z == m_playerLayer)
            return;
        m_playerField = QPoint(position.x, position.y);
        m_playerLayer = position.z;
        m_renderInfoStorage->onPlayerPositionChanged();
    });
    m_playerTimer.start();
}

TMinimapRendererQuickItem::~TMinimapRendererQuickItem()
{
    g_minimapRenderers.removeOne(this);
    for (auto& [id, caption] : m_captions)
        delete caption.item;
}

void TMinimapRendererQuickItem::notifyMarkersChanged()
{
    for (TMinimapRendererQuickItem* item : std::as_const(g_minimapRenderers))
        item->m_renderInfoStorage->onMinimapMarkersChanged();
}

TWorldMapCamera* TMinimapRendererQuickItem::worldCamera() const { return m_worldCamera; }

void TMinimapRendererQuickItem::setStorage(cyclopedia::TCyclopediaMapStorage* storage)
{
    m_storage = storage;
    m_renderInfoStorage->setMapStorage(storage);
}

void TMinimapRendererQuickItem::setHighlightedSubareas(const std::set<int>& subareas)
{
    if (m_highlighted == subareas)
        return;
    m_highlighted = subareas;
    std::set<quint16> shown;
    for (const int subarea : subareas)
        shown.insert(static_cast<quint16>(subarea));
    m_renderInfoStorage->setSubAreasToShow(shown);
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setHighlightedCoordinate(const Position& coordinate)
{
    if (m_highlightedCoordinate == coordinate)
        return;
    m_highlightedCoordinate = coordinate;
    m_renderInfoStorage->setSelectedPassage(worldmap::TWorldMapCoordinate(coordinate));
}

void TMinimapRendererQuickItem::setMissionZones(std::vector<TMissionZoneOverlay> zones)
{
    m_missionZones.clear();
    m_missionZoneImages.clear();
    for (TMissionZoneOverlay& zone : zones) {
        if (zone.id <= 0 || zone.file.isEmpty())
            continue;
        m_missionZones[zone.id] = std::move(zone);
    }
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setMissionCaptions(std::vector<TMissionCaption> captions)
{
    m_missionCaptions = std::move(captions);
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setShowCyclopediaSubareaCaptions(const bool show)
{
    if (show == m_showCyclopediaSubareaCaptions)
        return;
    m_showCyclopediaSubareaCaptions = show;
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setMinimapTileStorage(minimap::TMinimapTileStorage* storage) { g_minimapTiles = storage; }

void TMinimapRendererQuickItem::onCameraViewportChanged()
{
    m_drawCommandsOutdated = true;
    polish();
}

void TMinimapRendererQuickItem::onRenderInfosChanged()
{
    m_drawCommandsOutdated = true;
    polish();
}

void TMinimapRendererQuickItem::onRescalingTimerTimeout()
{
    m_rescaling = true;
    polish();
}

void TMinimapRendererQuickItem::updateRenderInfoSettings()
{
    minimap::TMinimapRenderInfoStorage& renderInfos = *m_renderInfoStorage;
    bool changed = false;
    if (renderInfos.showNPCMarkers() != m_showNPCMarkers) {
        renderInfos.setShowNPCMarkers(m_showNPCMarkers);
        changed = true;
    }
    if (renderInfos.showPassages() != m_showPassages) {
        renderInfos.setShowPassages(m_showPassages);
        changed = true;
    }
    if (renderInfos.showHouses() != m_showHouses) {
        renderInfos.setShowHouses(m_showHouses);
        changed = true;
    }
    if (renderInfos.showViewpoints() != m_showExploreOverlay) {
        renderInfos.setShowViewpoints(m_showExploreOverlay);
        changed = true;
    }
    if (renderInfos.minimapMarkersFilter() != m_minimapMarkersFilter) {
        renderInfos.setMinimapMarkersFilter(m_minimapMarkersFilter);
        changed = true;
    }
    if (changed)
        renderInfos.markMarkersOutdated();
    renderInfos.setTileType(static_cast<minimap::TMinimapRenderInfoStorage::ETileType>(m_tileType));

    minimap::TMinimapRendererScenegraphDrawCommandProvider& provider = *m_drawCommandProvider;
    if (provider.showAreaCaptions() != m_showAreaCaptions)
        provider.setShowAreaCaptions(m_showAreaCaptions);
    if (provider.drawSubAreaOverlays() != m_showZones) {
        provider.setDrawSubAreaOverlays(m_showZones);
        m_drawCommandsOutdated = true;
    }
    // Host: the level separator setting is a percentage.
    const float alpha = static_cast<float>(std::clamp(m_levelSeparator, 0, 100)) / 100.0f;
    if (provider.levelSeparatorAlpha() != alpha) {
        provider.setLevelSeparatorAlpha(alpha);
        m_drawCommandsOutdated = true;
        polish();
    }
}

void TMinimapRendererQuickItem::componentComplete()
{
    cip::tibia::qmlcomponents::TMinimapRendererQuickItem::componentComplete();
    if (QQmlEngine* engine = qmlEngine(this))
        m_captionComponent = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/AreaCaptionTemplate.qml")), this);
    scheduleUpdate();
}

void TMinimapRendererQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TMinimapRendererQuickItem::geometryChange(newGeometry, oldGeometry);
    m_worldCamera->setViewportSize(newGeometry.size());
    scheduleUpdate();
}

void TMinimapRendererQuickItem::scheduleUpdate()
{
    m_drawCommandsOutdated = true;
    polish();
}

void TMinimapRendererQuickItem::requestRebuild()
{
    m_rebuildRequested = true;
    scheduleUpdate();
}

void TMinimapRendererQuickItem::updatePolish()
{
    if (m_rebuildRequested) {
        m_rebuildRequested = false;
        m_zoneImages.clear();
        m_missionZoneImages.clear();
    }
    m_worldCamera->applyPendingCenter();
    if (!m_drawCommandsOutdated && !m_rescaling)
        return;
    m_drawCommandsOutdated = false;
    m_rescaling = false;
    updateDrawCommands();
}

void TMinimapRendererQuickItem::updateDrawCommands()
{
    if (!m_drawCommandProvider)
        return;
    // Host: the game client may install its static map data after the item exists.
    m_renderInfoStorage->setStaticDataStorage(&TStaticData::instance());
    // The view's size in fields at the camera's scale.
    const double scale = m_worldCamera->camera()->scaleFactor();
    const worldmap::TWorldMapExtent viewExtent(static_cast<int>(static_cast<float>(width() / scale) * 0.03125f),
                                               static_cast<int>(static_cast<float>(height() / scale) * 0.03125f), 7);
    m_drawCommandProvider->setViewExtent(viewExtent);
    m_drawCommandProvider->updateSpriteDrawCommands();
    updateCaptions();
    // Host: the pack's mission zones and the marker rectangles under the mouse.
    m_missionZoneDraws.clear();
    if (m_showZones)
        addMissionZones(m_missionZoneDraws);
    updateMarkerHits();
    update();
}

void TMinimapRendererQuickItem::addMissionZones(std::vector<TImageDraw>& draws)
{
    const auto& data = TStaticData::instance();
    const QRectF viewport = boundingRect();
    const int z = m_worldCamera->layer();
    std::vector<int> layers;
    if (z <= GroundLayer && m_tileType != ETileType::LocalMinimap) {
        for (int layer = GroundLayer; layer >= z; --layer)
            layers.push_back(layer);
    } else {
        layers.push_back(z);
    }
    const qreal opacity = renderer::highlightStrength();
    for (const int layer : layers) {
        for (const int subareaId : m_highlighted) {
            if (const auto* subArea = data.subArea(static_cast<quint16>(subareaId)); subArea && !subArea->fileName.isEmpty()) {
                // The CipSoft overlays show the ground floor's subareas above ground.
                if (subArea->extent.origin.z == layer && layer == GroundLayer && z <= GroundLayer)
                    continue;
            }
            const auto zoneIt = m_missionZones.find(subareaId);
            if (zoneIt == m_missionZones.end())
                continue;
            const TMissionZoneOverlay& zone = zoneIt->second;
            if (zone.z != layer || zone.fields.isEmpty() || zone.file.isEmpty())
                continue;
            const QRect& fields = zone.fields;
            const QRectF target(m_worldCamera->pixelOf(fields.x(), fields.y(), layer),
                                m_worldCamera->pixelOf(fields.x() + fields.width(), fields.y() + fields.height(), layer));
            if (!target.intersects(viewport))
                continue;
            const QImage mask = missionZoneImage(zone);
            if (mask.isNull())
                continue;
            draws.push_back({ mask, target, QRectF(QPointF(0, 0), QSizeF(fields.size())), true, opacity });
        }
    }
}

// CIP (effect 1) with 0xffffe137: the mask's alpha in the zone colour.
QImage TMinimapRendererQuickItem::zoneImage(const int subareaId, const QImage& mask)
{
    if (const auto it = m_zoneImages.find(subareaId); it != m_zoneImages.end())
        return it->second;
    const QColor zone = QColor::fromRgba(0xffffe137);
    QImage colored(mask.size(), QImage::Format_ARGB32);
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x)
            colored.setPixel(x, y, qRgba(zone.red(), zone.green(), zone.blue(), qAlpha(mask.pixel(x, y)) * zone.alpha() / 255));
    }
    m_zoneImages.emplace(subareaId, colored);
    return colored;
}

QImage TMinimapRendererQuickItem::missionZoneImage(const TMissionZoneOverlay& zone)
{
    if (const auto it = m_missionZoneImages.find(zone.id); it != m_missionZoneImages.end())
        return it->second;
    const std::string path = tibia::files::data().assetsRoot()
        + zone.file.toStdString();
    std::string data;
    try {
        data = tibia::files::data().readFileContents(path);
    } catch (const std::exception&) {
        m_missionZoneImages.emplace(zone.id, QImage{});
        return {};
    }
    const QImage mask = tibia::assets::decodeCipBmpImage(QByteArray(data.data(), qsizetype(data.size())));
    if (mask.isNull()) {
        m_missionZoneImages.emplace(zone.id, QImage{});
        return {};
    }
    const QImage colored = zoneImage(zone.id, mask);
    m_missionZoneImages[zone.id] = colored;
    return colored;
}

void TMinimapRendererQuickItem::ensureMarkerIcons()
{
    const auto load = [](QImage& image, const char* path, const char* fallback) {
        if (!image.isNull())
            return;
        image = QImage(QString::fromUtf8(path));
        if (image.isNull() && fallback)
            image = QImage(QString::fromUtf8(fallback));
    };
    load(m_npcIcon, ":/images/icon-map-npc.png", nullptr);
    load(m_playerIcon, ":/images/icon-map-player.png", nullptr);
    load(m_houseIcon, ":/images/icon-map-house.png", ":/qt/qml/qmlcomponents/qml/images/icon-map-house.png");
    load(m_passageIcon, ":/images/icon-map-passage-idle.png",
         ":/qt/qml/qmlcomponents/qml/images/icon-map-passage-idle.png");
    load(m_passageSelectedIcon, ":/images/icon-map-passage-selected.png",
         ":/qt/qml/qmlcomponents/qml/images/icon-map-passage-selected.png");
    load(m_raidIcon, ":/images/icon-raid.png", nullptr);
    if (m_flagStrip.isNull()) {
        m_flagStrip = QImage(QStringLiteral(":/images/minimap/markers.png"));
        if (m_flagStrip.isNull())
            m_flagStrip = QImage(QStringLiteral(":/qt/qml/qmlcomponents/qml/images/minimap/markers.png"));
    }
}

void TMinimapRendererQuickItem::updateMarkerHits()
{
    ensureMarkerIcons();
    const int z = m_worldCamera->layer();
    const QRectF viewport = boundingRect();
    const qreal halfField = SubfieldsPerField * 0.5 * m_worldCamera->scaleFactor();
    m_markers.clear();
    const gamewindow::TTooltipTextVisitor tooltipText(&cyclopedia::TCyclopediaMapStaticDataStorage::instance());
    const auto addPin = [&](const QImage& icon, const QColor&, const int x, const int y, const int layer,
                            const QString& tooltip, const int kind, const quint32 houseId) {
        const TMarkerHit hit{ kind, Position(x, y, static_cast<uint8_t>(layer)), houseId };
        const QPointF center = m_worldCamera->pixelOf(x, y, layer) + QPointF(halfField, halfField);
        if (!icon.isNull()) {
            const QRectF target(center - QPointF(icon.width() * 0.5, icon.height() * 0.5), icon.size());
            if (!target.intersects(viewport))
                return;
            m_markers.push_back({ target, tooltip, hit });
            return;
        }
        const qreal size = 6 * m_worldCamera->scaleFactor() * SubfieldsPerField / 8.0;
        const QRectF target(center.x() - size * 0.5, center.y() - size * 0.5, size, size);
        if (!target.intersects(viewport))
            return;
        m_markers.push_back({ target, tooltip, hit });
    };
    if (m_showNPCMarkers) {
        for (const auto& npc : TStaticData::instance().npcs()) {
            if (npc.coordinate.z == z)
                addPin(m_npcIcon, QColor(0x33, 0x99, 0xff), npc.coordinate.x, npc.coordinate.y, z, npc.name, TMarkerHit::NPCMarker, 0);
        }
    }
    // Explore overlay: viewpoint POIs of the discovery subarea (CIP pink pins).
    if (m_showExploreOverlay && m_storage) {
        for (const auto& point : m_storage->viewpoints()) {
            if (point.coordinate.z != z)
                continue;
            const qreal size = 6 * m_worldCamera->scaleFactor() * SubfieldsPerField / 8.0;
            const QPointF center = m_worldCamera->pixelOf(point.coordinate.x, point.coordinate.y, z) + QPointF(halfField, halfField);
            const QRectF target(center.x() - size * 0.5, center.y() - size * 0.5, size, size);
            if (target.intersects(viewport)) {
                m_markers.push_back({ target, tooltipText(worldmap::TViewpointMapAreaData{}),
                                      { TMarkerHit::Viewpoint,
                                        Position(point.coordinate.x, point.coordinate.y, static_cast<uint8_t>(point.coordinate.z)), 0 } });
            }
        }
    }
    // CIP THouseMarkerWorldMapAreaData: the house entrances of the static map
    // data; the tooltip is the house's name and its listing.
    if (m_showHouses) {
        const worldmap::TWorldMapCuboid floor(worldmap::TWorldMapCoordinate(0, 0, z), worldmap::TWorldMapExtent(0xffff, 0xffff, 1));
        for (const worldmap::TWorldMapAreaElement& element : TStaticData::instance().houseMarkersIn(floor)) {
            const auto* house = std::get_if<worldmap::THouseMarkerWorldMapAreaData>(&element.data);
            if (!house)
                continue;
            const QString tooltip = tooltipText(*house);
            addPin(m_houseIcon, QColor(0xe6, 0x96, 0x2d), element.area.origin.x, element.area.origin.y, z, tooltip,
                   TMarkerHit::HouseMarker, house->houseId);
        }
    }
    // CIP TActiveRaidMapAreaData: the raids the server announced.
    if (m_storage) {
        for (const worldmap::TWorldMapCoordinate& raid : m_storage->activeRaids()) {
            if (raid.z != z)
                continue;
            addPin(m_raidIcon, QColor(0xff, 0x33, 0x33), raid.x, raid.y, z, tooltipText(worldmap::TActiveRaidMapAreaData{}), TMarkerHit::ActiveRaid, 0);
        }
    }
    // User / bundled map flags (CIP TMinimapMarkerStorage owns the layer; the
    // static map data holds the client's marker storage).
    if (const minimap::TMinimapMarkerStorage* markers = TStaticData::instance().minimapMarkerStorage()) {
        const int flagSize = m_flagStrip.isNull() ? 0 : m_flagStrip.height();
        const int columns = flagSize > 0 ? std::max(1, m_flagStrip.width() / flagSize) : 1;
        const auto onFloor = [z](const worldmap::TWorldMapCoordinate& coordinate) { return coordinate.z == z; };
        for (const minimap::TMinimapMarkerPtr& marker : markers->markers(onFloor)) {
            if (marker->icon < m_minimapMarkersFilter.size() && !m_minimapMarkersFilter.at(marker->icon))
                continue;
            QImage icon;
            if (flagSize > 0 && marker->icon < minimap::MINIMAP_MARKER_ICONS) {
                const QRect cell((marker->icon % columns) * flagSize, (marker->icon / columns) * flagSize, flagSize,
                                 flagSize);
                if (m_flagStrip.rect().contains(cell))
                    icon = m_flagStrip.copy(cell);
            }
            addPin(icon, QColor(0xff, 0xcc, 0x33), marker->coordinate.x, marker->coordinate.y, z, marker->description,
                   TMarkerHit::MinimapMarker, 0);
        }
    }
    // CIP TPassageMapAreaData from 0xDD subtype 6 (TCyclopediaMapStorage). The tooltip
    // names the subareas the passage leads to.
    if (m_showPassages && m_storage) {
        for (const auto& passage : m_storage->passages()) {
            if (passage.coordinate.z != z)
                continue;
            worldmap::TPassageMapAreaData data;
            for (const int subAreaId : passage.subAreaIds)
                data.raceIds.push_back(static_cast<quint16>(subAreaId));
            const QString tooltip = tooltipText(data);
            // The selected passage's field is drawn with the selected icon.
            const bool selected = m_highlightedCoordinate == Position(passage.coordinate.x, passage.coordinate.y,
                                                                      static_cast<uint8_t>(passage.coordinate.z));
            addPin(selected ? m_passageSelectedIcon : m_passageIcon, QColor(0x66, 0xcc, 0xff), passage.coordinate.x,
                   passage.coordinate.y, z, tooltip, TMarkerHit::Passage, 0);
        }
    }
    if (const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession(); session && session->hasLocalPlayer()) {
        const Position position = session->localPlayerPosition();
        const bool drawn = position.z == z || (z <= GroundLayer && position.z > z && position.z <= GroundLayer && m_tileType != ETileType::LocalMinimap);
        if (position.isValid() && drawn)
            addPin(m_playerIcon, QColor(0x66, 0xff, 0x5e), position.x, position.y, position.z,
                   tooltipText(worldmap::TPlayerPositionMapAreaData{}), TMarkerHit::PlayerPosition, 0);
    }
}

std::vector<TMinimapRendererQuickItem::TMarkerHit> TMinimapRendererQuickItem::markersAt(const QPointF& point) const
{
    std::vector<TMarkerHit> hits;
    for (auto it = m_markers.rbegin(); it != m_markers.rend(); ++it) {
        if (it->rect.contains(point))
            hits.push_back(it->hit);
    }
    return hits;
}

QString TMinimapRendererQuickItem::markerTooltipAt(const QPointF& point) const
{
    for (auto it = m_markers.rbegin(); it != m_markers.rend(); ++it) {
        if (it->rect.contains(point))
            return it->tooltip;
    }
    return {};
}

// CIP : area captions fade into subarea captions between the
// scales 1/32 and 1/16.
void TMinimapRendererQuickItem::updateCaptions()
{
    // Area captions are QML items owned by the engine. A sprite rebuild can mark the
    // scene dirty from the render thread; creating them there hits
    // "Cannot create children for a parent that is in a different thread".
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this] { updateCaptions(); }, Qt::QueuedConnection);
        return;
    }
    std::set<int> shown;
    if (m_showAreaCaptions && m_captionComponent && m_captionComponent->isReady()) {
        const float scale = m_worldCamera->scaleFactor();
        const int layer = m_worldCamera->layer();
        float subareaOpacity = 0;
        if (scale > 1.0f / 32.0f)
            subareaOpacity = (scale - 1.0f / 32.0f) / (1.0f / 32.0f);
        if (scale >= 1.0f / 16.0f)
            subareaOpacity = 1.0f;
        const QRectF viewport = boundingRect();
        struct TLabel
        {
            int id;
            QString name;
            worldmap::TWorldMapCoordinate label;
            bool isSubarea;
        };
        std::vector<TLabel> labels;
        const auto& staticData = TStaticData::instance();
        for (const auto& [areaId, area] : staticData.areas())
            labels.push_back({ areaId, area.name, area.labelCoordinate, false });
        for (const auto& [subAreaId, subArea] : staticData.subAreas())
            labels.push_back({ subAreaId, subArea.name, subArea.labelCoordinate, true });
        for (const TLabel& area : labels) {
            if (area.label.x == 0 && area.label.y == 0 && area.label.z == 0)
                continue;
            // Only the CIP label's own floor. Underground text must not stack onto z7 —
            // but CIP may place a name on z7 even when the hunt is deeper (Cyclopolis).
            if (area.label.z != layer)
                continue;
            if (area.isSubarea && !m_showCyclopediaSubareaCaptions)
                continue;
            const float opacity = area.isSubarea ? subareaOpacity : 1.0f - subareaOpacity;
            if (opacity <= 0)
                continue;
            const QPointF topLeft = m_worldCamera->pixelOf(area.label.x, area.label.y, area.label.z);
            const QPointF bottomRight = m_worldCamera->pixelOf(area.label.x + 1, area.label.y + 1, area.label.z);
            const QRectF rect(topLeft, bottomRight);
            if (!rect.adjusted(-100, -50, 100, 50).intersects(viewport))
                continue;
            TCaption& caption = m_captions[area.id];
            if (!caption.item) {
                caption.controller = new TAreaCaptionController(area.id, !area.isSubarea, this);
                connect(caption.controller, &TAreaCaptionController::clicked, this, &TMinimapRendererQuickItem::areaClicked);
                QObject* object = m_captionComponent->beginCreate(qmlContext(this));
                caption.item = qobject_cast<QQuickItem*>(object);
                if (!caption.item) {
                    delete object;
                    delete caption.controller;
                    m_captions.erase(area.id);
                    continue;
                }
                caption.item->setParentItem(this);
                caption.item->setZ(1); // above the map MouseArea (z:0)
                caption.item->setProperty("controller", QVariant::fromValue<QObject*>(caption.controller));
                m_captionComponent->completeCreate();
            }
            QColor color(Qt::white);
            bool improvedRespawn = false;
            if (m_storage) {
                color = area.isSubarea ? m_storage->subareaColor(area.id) : m_storage->areaColor(area.id);
                improvedRespawn = !area.isSubarea && m_storage->isImprovedRespawnActive(area.id);
            }
            caption.controller->set(area.name, color, improvedRespawn);
            caption.item->setX(rect.x());
            caption.item->setY(rect.y());
            caption.item->setWidth(rect.width());
            caption.item->setHeight(rect.height());
            caption.item->setOpacity(opacity);
            caption.item->setZ(1);
            shown.insert(area.id);
        }
        // FO≥1 hunt pins only ("Name -N"). Never on the surface (z≤7) — even if the
        // catalog left center.z wrong — so "Edron Surroundings -1" cannot appear on z7.
        // FO0 / surface names come from CIP AreaData above (Cyclopolis, …).
        for (const TMissionCaption& pin : m_missionCaptions) {
            if (pin.id <= 0 || pin.name.isEmpty() || shown.contains(pin.id))
                continue;
            if (pin.z <= GroundLayer || pin.z != layer)
                continue;
            float opacity = subareaOpacity;
            if (m_highlighted.contains(pin.id))
                opacity = 1.0f;
            if (opacity <= 0)
                continue;
            const QPointF topLeft = m_worldCamera->pixelOf(pin.x, pin.y, pin.z);
            const QPointF bottomRight = m_worldCamera->pixelOf(pin.x + 1, pin.y + 1, pin.z);
            const QRectF rect(topLeft, bottomRight);
            if (!rect.adjusted(-100, -50, 100, 50).intersects(viewport))
                continue;
            TCaption& caption = m_captions[pin.id];
            if (!caption.item) {
                caption.controller = new TAreaCaptionController(pin.id, false, this);
                connect(caption.controller, &TAreaCaptionController::clicked, this, &TMinimapRendererQuickItem::areaClicked);
                QObject* object = m_captionComponent->beginCreate(qmlContext(this));
                caption.item = qobject_cast<QQuickItem*>(object);
                if (!caption.item) {
                    delete object;
                    delete caption.controller;
                    m_captions.erase(pin.id);
                    continue;
                }
                caption.item->setParentItem(this);
                caption.item->setZ(1);
                caption.item->setProperty("controller", QVariant::fromValue<QObject*>(caption.controller));
                m_captionComponent->completeCreate();
            }
            QColor color(Qt::white);
            if (m_storage)
                color = m_storage->subareaColor(pin.id);
            caption.controller->set(pin.name, color, false);
            caption.item->setX(rect.x());
            caption.item->setY(rect.y());
            caption.item->setWidth(rect.width());
            caption.item->setHeight(rect.height());
            caption.item->setOpacity(opacity);
            caption.item->setZ(1);
            shown.insert(pin.id);
        }
    }
    for (auto it = m_captions.begin(); it != m_captions.end();) {
        if (shown.contains(it->first)) {
            ++it;
            continue;
        }
        delete it->second.item;
        delete it->second.controller;
        it = m_captions.erase(it);
    }
}

QSGNode* TMinimapRendererQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* root = static_cast<TRendererNode*>(oldNode);
    if (!root) {
        QQuickWindow* quickWindow = window();
        if (!quickWindow)
            return nullptr;
        auto nodeProvider = std::make_shared<renderer::TScenegraphNodeProvider>(quickWindow);
        root = new TRendererNode(new TQSGScenegraphDrawCommandsNode(
            renderer::TSpriteTextureProviderManager::sharedProvider(quickWindow), std::move(nodeProvider)));
    }
    bool animated = root->drawCommands->update(m_drawCommandProvider->rootDrawCommand());

    // Host: the pack's mission zones over the tree, pulsing like the overlays.
    root->clearMissionZones();
    std::unordered_map<qint64, QSGTexture*> used;
    for (const TImageDraw& image : m_missionZoneDraws) {
        const qint64 key = image.image.cacheKey();
        QSGTexture* texture = nullptr;
        if (const auto it = used.find(key); it != used.end()) {
            texture = it->second;
        } else if (const auto cached = root->textures.find(key); cached != root->textures.end()) {
            texture = cached->second;
            root->textures.erase(cached);
        } else {
            texture = window()->createTextureFromImage(image.image);
        }
        if (!texture)
            continue;
        used.emplace(key, texture);
        QSGImageNode* node = window()->createImageNode();
        node->setTexture(texture);
        node->setOwnsTexture(false);
        node->setRect(image.target);
        node->setSourceRect(image.source);
        node->setFiltering(image.smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        auto* opacityNode = new QSGOpacityNode;
        opacityNode->setOpacity(image.opacity);
        opacityNode->appendChildNode(node);
        root->missionZones->appendChildNode(opacityNode);
        animated = true;
    }
    deleteTextures(root->textures);
    root->textures = std::move(used);

    // A command changing every frame or a sprite still loading: build again.
    if (animated) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_drawCommandsOutdated = true;
                polish();
            },
            Qt::QueuedConnection);
    }
    return root;
}

}
