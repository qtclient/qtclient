#include "minimaprendererquickitem.h"

#include "qmlcomponents/worldmapqmltypes.h"

#include "client/gameclient.h"
#include "client/gamecore.h"
#include "cyclopedia/cyclopediamapdata.h"
#include "minimap/minimapmanager.h"
#include "minimap/minimaptile.h"
#include "minimap/minimaptilestorage.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/minimap.h>
#include <tibia/assets/things.h>
#include <framework/core/clock.h>
#include <framework/core/resourcemanager.h>
#include <framework/graphics/image.h>

#include <QList>
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

namespace tibia::qmlcomponents {

namespace {

using EFileType = cyclopedia::TCyclopediaMapStaticData::EFileType;
using ETileType = cip::tibia::qmlcomponents::TMinimapRendererQuickItem::EMinimapTileType;

constexpr int SubfieldsPerField = 32;
constexpr int GroundLayer = 7;
constexpr int MaxLayer = 15;

minimap::TMinimapTileStorage* g_minimapTiles = nullptr;
QList<TMinimapRendererQuickItem*> g_minimapRenderers;

/// CIP 0x1010c0b00: the viewport's fill per tile type.
QColor backgroundColor(const ETileType type)
{
    switch (type) {
        case ETileType::Satellite: return QColor(0x27, 0x4c, 0xa5);
        case ETileType::Minimap: return QColor(0x33, 0x66, 0x99);
        default: return QColor(Qt::black);
    }
}

/// Field coordinates are positive, so the subfield floor division is a shift.
int fieldOf(const int subfield) { return subfield >> 5; }

/// The scene graph root: owns the textures of the images drawn last frame.
void deleteTextures(const std::unordered_map<qint64, QSGTexture*>& textures)
{
    for (const auto& [key, texture] : textures)
        delete texture;
}

class TRendererNode final : public QSGNode
{
public:
    ~TRendererNode() override { deleteTextures(textures); }
    void clear()
    {
        while (QSGNode* child = firstChild()) {
            removeChildNode(child);
            delete child;
        }
    }
    std::unordered_map<qint64, QSGTexture*> textures;
};

}

TWorldMapCamera::TWorldMapCamera(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCameraQmlType(parent)
{
    m_topLeftSubfieldCoordinate = new TWorldMapSubfieldCoordinateQmlType(this);
    m_topLeftSubfieldCoordinate->setZ(GroundLayer);
    m_scaleFactor = 1.0f / 16.0f;
}

QPoint TWorldMapCamera::topLeft() const
{
    return { m_topLeftSubfieldCoordinate->subfieldX(), m_topLeftSubfieldCoordinate->subfieldY() };
}

int TWorldMapCamera::layer() const { return m_topLeftSubfieldCoordinate->z(); }

void TWorldMapCamera::setViewportSize(const QSizeF& size) { m_viewport = size; }

QPointF TWorldMapCamera::pixelOf(const int x, const int y, const int layer) const
{
    const QPoint origin = topLeft() + QPoint(this->layer(), this->layer()) * SubfieldsPerField;
    return QPointF((x + layer) * SubfieldsPerField - origin.x(), (y + layer) * SubfieldsPerField - origin.y()) * m_scaleFactor;
}

QPoint TWorldMapCamera::fieldAt(const QPointF& point) const
{
    const QPoint subfield = subfieldAt(point);
    return { fieldOf(subfield.x()), fieldOf(subfield.y()) };
}

QPoint TWorldMapCamera::subfieldAt(const QPointF& point) const
{
    return topLeft() + QPoint(static_cast<int>(std::lround(point.x() / m_scaleFactor)), static_cast<int>(std::lround(point.y() / m_scaleFactor)));
}

void TWorldMapCamera::placeAt(const QPoint& subfield, const QPointF& point)
{
    setTopLeft(subfield - QPoint(static_cast<int>(std::lround(point.x() / m_scaleFactor)), static_cast<int>(std::lround(point.y() / m_scaleFactor))), layer());
}

void TWorldMapCamera::setTopLeft(const QPoint& subfield, const int layer)
{
    m_topLeftSubfieldCoordinate->setSubfieldX(subfield.x());
    m_topLeftSubfieldCoordinate->setSubfieldY(subfield.y());
    m_topLeftSubfieldCoordinate->setZ(layer);
}

void TWorldMapCamera::setTopLeftSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v)
{
    if (!v)
        return;
    setTopLeft({ v->subfieldX(), v->subfieldY() }, v->z());
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::setScaleFactor(const float v)
{
    if (v <= 0 || v == m_scaleFactor)
        return;
    const QPoint reference = subfieldAt(m_referencePoint);
    m_scaleFactor = v;
    placeAt(reference, m_referencePoint);
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::setReferencePoint(const QPoint& v)
{
    m_referenceSubfield = subfieldAt(v);
    if (m_referencePoint == v)
        return;
    m_referencePoint = v;
    Q_EMIT cameraViewportChanged();
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TWorldMapCamera::coordinateAtPoint(const QPoint& Value)
{
    const QPoint field = fieldAt(Value);
    auto* coordinate = new TWorldMapCoordinateQmlType;
    coordinate->setX(field.x());
    coordinate->setY(field.y());
    coordinate->setZ(layer());
    QQmlEngine::setObjectOwnership(coordinate, QQmlEngine::JavaScriptOwnership);
    return coordinate;
}

void TWorldMapCamera::changeLayerWithIsometricProjection(int NewLayer)
{
    NewLayer = std::clamp(NewLayer, 0, MaxLayer);
    const int oldLayer = layer();
    if (NewLayer == oldLayer)
        return;
    const QPoint subfield = topLeft();
    const auto project = [&](const int value) { return (fieldOf(value) + oldLayer - NewLayer) * SubfieldsPerField + (value & (SubfieldsPerField - 1)); };
    setTopLeft({ project(subfield.x()), project(subfield.y()) }, NewLayer);
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::moveDistanceRelativeToReferencePoint(const QPoint& Distance)
{
    placeAt(m_referenceSubfield + (subfieldAt(Distance) - topLeft()), m_referencePoint);
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::centerAtSubfieldCoordinateDelayed(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pSubfieldCoordinate)
{
    if (pSubfieldCoordinate)
        centerDelayed(pSubfieldCoordinate->subfieldX(), pSubfieldCoordinate->subfieldY(), pSubfieldCoordinate->z());
}

void TWorldMapCamera::centerDelayed(const int subfieldX, const int subfieldY, const int layer)
{
    m_pendingCenter = { subfieldX, subfieldY };
    m_pendingLayer = layer;
    m_hasPendingCenter = true;
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::applyPendingCenter()
{
    if (!m_hasPendingCenter || m_viewport.isEmpty())
        return;
    m_hasPendingCenter = false;
    const QPointF center(std::round(m_viewport.width() * 0.5), std::round(m_viewport.height() * 0.5));
    setTopLeft(topLeft(), m_pendingLayer);
    placeAt(m_pendingCenter, center);
    Q_EMIT cameraViewportChanged();
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
    m_minimapMarkersFilter = QList<bool>(20, true);
    ensureMarkerIcons();
    connect(m_worldCamera, &TWorldMapCamera::cameraViewportChanged, this, &TMinimapRendererQuickItem::scheduleUpdate);
    connect(this, &TMinimapRendererQuickItem::propertyChanged, this, [this] {
        syncZoneBlinkTimer();
        scheduleUpdate();
    });
    connect(&cyclopedia::TCyclopediaMapStaticData::instance(), &cyclopedia::TCyclopediaMapStaticData::imageLoaded, this,
            &TMinimapRendererQuickItem::scheduleUpdate);
    m_playerTimer.setInterval(250);
    connect(&m_playerTimer, &QTimer::timeout, this, [this] {
        const LocalPlayerPtr& player = g_game.getLocalPlayer();
        if (!player)
            return;
        const Position position = player->getPosition();
        if (QPoint(position.x, position.y) != m_playerField || position.z != m_playerLayer)
            scheduleUpdate();
    });
    m_playerTimer.start();
    // CIP updatePaintNode re-polishes while zone overlays are drawn so the
    // shared highlight alpha (FUN_100f7ffb0) can pulse.
    m_zoneBlinkTimer.setInterval(33);
    connect(&m_zoneBlinkTimer, &QTimer::timeout, this, &TMinimapRendererQuickItem::scheduleUpdate);
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
        item->scheduleUpdate();
}

TWorldMapCamera* TMinimapRendererQuickItem::worldCamera() const { return m_worldCamera; }

void TMinimapRendererQuickItem::setStorage(cyclopedia::TCyclopediaMapStorage* storage)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = storage;
    if (storage)
        connect(storage, &cyclopedia::TCyclopediaMapStorage::changed, this, &TMinimapRendererQuickItem::scheduleUpdate);
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setHighlightedSubareas(const std::set<int>& subareas)
{
    if (m_highlighted == subareas)
        return;
    m_highlighted = subareas;
    syncZoneBlinkTimer();
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setExpeditionZones(std::vector<TExpeditionZoneOverlay> zones)
{
    m_expeditionZones.clear();
    m_expeditionZoneImages.clear();
    for (TExpeditionZoneOverlay& zone : zones) {
        if (zone.id <= 0 || zone.file.isEmpty())
            continue;
        m_expeditionZones[zone.id] = std::move(zone);
    }
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setExpeditionCaptions(std::vector<TExpeditionCaption> captions)
{
    m_expeditionCaptions = std::move(captions);
    scheduleUpdate();
}

void TMinimapRendererQuickItem::setShowCyclopediaSubareaCaptions(const bool show)
{
    if (show == m_showCyclopediaSubareaCaptions)
        return;
    m_showCyclopediaSubareaCaptions = show;
    scheduleUpdate();
}

void TMinimapRendererQuickItem::syncZoneBlinkTimer()
{
    if (m_showZones && !m_highlighted.empty()) {
        if (!m_zoneBlinkTimer.isActive())
            m_zoneBlinkTimer.start();
    } else if (m_zoneBlinkTimer.isActive()) {
        m_zoneBlinkTimer.stop();
    }
}

float TMinimapRendererQuickItem::zoneHighlightOpacity()
{
    // CIP FUN_100f7ffb0: same pulse as world-map sprite highlights.
    return static_cast<float>((std::sin(g_clock.millis() / 100.0) * 0.15 + 0.65) * 0.75);
}

void TMinimapRendererQuickItem::setMinimapTileStorage(minimap::TMinimapTileStorage* storage) { g_minimapTiles = storage; }

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
    polish();
}

void TMinimapRendererQuickItem::updatePolish()
{
    m_worldCamera->applyPendingCenter();
    buildDrawList();
    updateCaptions();
    update();
}

void TMinimapRendererQuickItem::buildDrawList()
{
    m_drawList.clear();
    const int z = m_worldCamera->layer();
    const QRectF viewport = boundingRect();
    TDraw background;
    background.rects.push_back({ viewport, backgroundColor(m_tileType) });
    m_drawList.push_back(std::move(background));

    // CIP 0x1010c0b00: the floors from the ground up to the viewed one when
    // it is above ground; each deeper floor is darkened by the separator.
    std::vector<int> layers;
    if (z <= GroundLayer && m_tileType != ETileType::LocalMinimap) {
        for (int layer = GroundLayer; layer >= z; --layer)
            layers.push_back(layer);
    } else {
        layers.push_back(z);
    }
    const qreal separator = m_levelSeparator / 100.0;
    for (const int layer : layers) {
        TDraw draw;
        if (m_tileType == ETileType::LocalMinimap)
            addLocalMinimap(draw, layer);
        else
            addTiles(draw, layer);
        if (m_showZones)
            addZones(draw, layer);
        if (layer > z && separator > 0 && z <= GroundLayer) {
            QColor shade(Qt::black);
            shade.setAlphaF(static_cast<float>(std::min<qreal>(separator, 1.0)));
            draw.rects.push_back({ viewport, shade });
        }
        m_drawList.push_back(std::move(draw));
    }
    TDraw markers;
    addMarkers(markers);
    m_drawList.push_back(std::move(markers));
}

void TMinimapRendererQuickItem::addTiles(TDraw& draw, const int layer)
{
    auto& data = cyclopedia::TCyclopediaMapStaticData::instance();
    const EFileType type = m_tileType == ETileType::Satellite ? EFileType::Satellite : EFileType::Minimap;
    // CIP 0x100a88ca0: one level of detail for the whole viewport.
    const double scale = data.levelOfDetail(type, m_worldCamera->scaleFactor());
    const QRectF viewport = boundingRect();
    const auto& files = data.files();
    for (size_t index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        if (file.type != type || file.scale != scale || file.topLeft.z != layer)
            continue;
        const QPointF topLeft = m_worldCamera->pixelOf(file.topLeft.x, file.topLeft.y, layer);
        const QPointF bottomRight = m_worldCamera->pixelOf(file.topLeft.x + file.width, file.topLeft.y + file.height, layer);
        const QRectF target(topLeft, bottomRight);
        if (!target.intersects(viewport))
            continue;
        const QImage image = data.image(index);
        if (image.isNull())
            continue;
        const qreal pixelsPerField = file.scale * SubfieldsPerField;
        draw.images.push_back({ image, target, QRectF(0, 0, file.width * pixelsPerField, file.height * pixelsPerField), type == EFileType::Satellite });
    }
}

void TMinimapRendererQuickItem::addLocalMinimap(TDraw& draw, const int layer)
{
    constexpr int TileSize = minimap::TMinimapTile::Size;
    const QPoint topLeft = m_worldCamera->fieldAt(QPointF(0, 0));
    const QPoint bottomRight = m_worldCamera->fieldAt(QPointF(width(), height()));
    for (int y = topLeft.y() & ~(TileSize - 1); y <= bottomRight.y(); y += TileSize) {
        for (int x = topLeft.x() & ~(TileSize - 1); x <= bottomRight.x(); x += TileSize) {
            QImage image;
            if (g_minimapTiles) {
                if (const auto tile = g_minimapTiles->tileAt(Position(x, y, layer)))
                    image = tile->colorImage();
            }
            if (image.isNull()) {
                const qint64 key = (qint64(x) << 32) | (qint64(y) << 8) | layer;
                auto it = m_localTiles.find(key);
                if (it == m_localTiles.end()) {
                    const QString file = minimap::TMinimapManager::minimapDir()
                        + QStringLiteral("/Minimap_Color_%1_%2_%3.png").arg(x).arg(y).arg(layer);
                    it = m_localTiles.emplace(key, QImage(file)).first;
                }
                image = it->second;
            }
            if (image.isNull())
                continue;
            const QRectF target(m_worldCamera->pixelOf(x, y, layer), m_worldCamera->pixelOf(x + TileSize, y + TileSize, layer));
            draw.images.push_back({ image, target, QRectF(0, 0, TileSize, TileSize), false });
        }
    }
}

void TMinimapRendererQuickItem::addZones(TDraw& draw, const int layer)
{
    auto& data = cyclopedia::TCyclopediaMapStaticData::instance();
    const QRectF viewport = boundingRect();
    // CIP 0x1010b9af0 builds the zone with 0xffffe137; draw opacity is
    // DAT_1077b32c8 (FUN_100f80020) so the selection blinks like mouse
    // / attack highlights on the game world map.
    const qreal opacity = zoneHighlightOpacity();
    for (const int subareaId : m_highlighted) {
        const int index = data.subareaFile(subareaId);
        if (index < 0)
            continue;
        const auto& file = data.files()[static_cast<size_t>(index)];
        if (file.topLeft.z != layer)
            continue;
        const QRectF target(m_worldCamera->pixelOf(file.topLeft.x, file.topLeft.y, layer),
                            m_worldCamera->pixelOf(file.topLeft.x + file.width, file.topLeft.y + file.height, layer));
        if (!target.intersects(viewport))
            continue;
        // Sync so the blink appears on the same click that selected the subarea.
        const QImage mask = data.imageSync(static_cast<size_t>(index));
        if (mask.isNull())
            continue;
        // Linear filtering matches CIP cyclopedia zone edges when the mask is scaled.
        draw.images.push_back({ zoneImage(subareaId, mask), target, QRectF(0, 0, file.width, file.height), true, opacity });
    }
    for (const int subareaId : m_highlighted) {
        const int cipIndex = data.subareaFile(subareaId);
        if (cipIndex >= 0) {
            const auto& file = data.files()[static_cast<size_t>(cipIndex)];
            if (file.topLeft.z == layer)
                continue; // CIP mask already drawn above.
        }
        const auto zoneIt = m_expeditionZones.find(subareaId);
        if (zoneIt == m_expeditionZones.end())
            continue;
        const TExpeditionZoneOverlay& zone = zoneIt->second;
        if (zone.z != layer || zone.w <= 0 || zone.h <= 0 || zone.file.isEmpty())
            continue;
        const QRectF target(m_worldCamera->pixelOf(zone.x, zone.y, layer),
                            m_worldCamera->pixelOf(zone.x + zone.w, zone.y + zone.h, layer));
        if (!target.intersects(viewport))
            continue;
        const QImage mask = expeditionZoneImage(zone);
        if (mask.isNull())
            continue;
        draw.images.push_back({ mask, target, QRectF(0, 0, zone.w, zone.h), true, opacity });
    }
}

// CIP 0x100ff6790 (effect 1) with 0xffffe137: the mask's alpha in the zone colour.
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

QImage TMinimapRendererQuickItem::expeditionZoneImage(const TExpeditionZoneOverlay& zone)
{
    if (const auto it = m_expeditionZoneImages.find(zone.id); it != m_expeditionZoneImages.end())
        return it->second;
    const std::string path = client::gamecore::assetsRoot(client::TGameClient::ClientVersion)
        + zone.file.toStdString();
    std::string data;
    try {
        data = tibia::files::data().readFileContents(path);
    } catch (const std::exception& e) {
        qWarning("expedition zone: %s: %s", path.c_str(), e.what());
        m_expeditionZoneImages.emplace(zone.id, QImage{});
        return {};
    }
    const ImagePtr image = Image::fromCipBmpData(data);
    if (!image || image->getBpp() != 4) {
        m_expeditionZoneImages.emplace(zone.id, QImage{});
        return {};
    }
    const Size size = image->getSize();
    const QImage mask = QImage(image->getPixelData(), size.width(), size.height(), size.width() * 4, QImage::Format_RGBA8888).copy();
    const QImage colored = zoneImage(zone.id, mask);
    m_expeditionZoneImages[zone.id] = colored;
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
    if (m_flagStrip.isNull()) {
        m_flagStrip = QImage(QStringLiteral(":/images/minimap/markers.png"));
        if (m_flagStrip.isNull())
            m_flagStrip = QImage(QStringLiteral(":/qt/qml/qmlcomponents/qml/images/minimap/markers.png"));
    }
}

void TMinimapRendererQuickItem::addMarkers(TDraw& draw)
{
    ensureMarkerIcons();
    const int z = m_worldCamera->layer();
    const QRectF viewport = boundingRect();
    const qreal halfField = SubfieldsPerField * 0.5 * m_worldCamera->scaleFactor();
    m_markers.clear();
    const auto addPin = [&](const QImage& icon, const QColor& fallback, const int x, const int y, const int layer,
                            const QString& tooltip) {
        const QPointF center = m_worldCamera->pixelOf(x, y, layer) + QPointF(halfField, halfField);
        if (!icon.isNull()) {
            const QRectF target(center - QPointF(icon.width() * 0.5, icon.height() * 0.5), icon.size());
            if (!target.intersects(viewport))
                return;
            draw.images.push_back({ icon, target, QRectF(QPointF(0, 0), icon.size()), false });
            m_markers.push_back({ target, tooltip });
            return;
        }
        const qreal size = 6 * m_worldCamera->scaleFactor() * SubfieldsPerField / 8.0;
        const QRectF target(center.x() - size * 0.5, center.y() - size * 0.5, size, size);
        if (!target.intersects(viewport))
            return;
        draw.rects.push_back({ target, fallback });
        m_markers.push_back({ target, tooltip });
    };
    if (m_showNPCMarkers) {
        for (const auto& npc : cyclopedia::TCyclopediaMapStaticData::instance().npcs()) {
            if (npc.coordinate.z == z)
                addPin(m_npcIcon, QColor(0x33, 0x99, 0xff), npc.coordinate.x, npc.coordinate.y, z, npc.name);
        }
    }
    // Explore overlay: viewpoint POIs of the discovery subarea (CIP pink pins).
    if (m_showExploreOverlay && m_storage) {
        for (const auto& point : m_storage->viewpoints()) {
            if (point.coordinate.z != z)
                continue;
            const QColor color = point.state != 0 ? QColor(0x66, 0xff, 0x5e) : QColor(0xcc, 0x33, 0x91);
            const qreal size = 6 * m_worldCamera->scaleFactor() * SubfieldsPerField / 8.0;
            const QPointF center = m_worldCamera->pixelOf(point.coordinate.x, point.coordinate.y, z) + QPointF(halfField, halfField);
            const QRectF target(center.x() - size * 0.5, center.y() - size * 0.5, size, size);
            if (target.intersects(viewport))
                draw.rects.push_back({ target, color });
        }
    }
    // CIP THouseMarkerWorldMapAreaData: house positions from staticdata.dat.
    if (m_showHouses) {
        for (const CyclopediaHouseType& house : g_things.getCyclopediaHouses()) {
            if (house.position.z != z)
                continue;
            addPin(m_houseIcon, QColor(0xe6, 0x96, 0x2d), house.position.x, house.position.y, z,
                   QString::fromStdString(house.name));
        }
    }
    // User / bundled map flags (CIP TMinimapMarkerStorage → g_minimap).
    {
        const int flagSize = m_flagStrip.isNull() ? 0 : m_flagStrip.height();
        const int columns = flagSize > 0 ? std::max(1, m_flagStrip.width() / flagSize) : 1;
        for (const MinimapMarker& marker : g_minimap.getFloorMarkers(static_cast<uint8_t>(z))) {
            if (marker.icon < m_minimapMarkersFilter.size() && !m_minimapMarkersFilter.at(marker.icon))
                continue;
            QImage icon;
            if (flagSize > 0 && marker.icon < MINIMAP_MARKER_ICONS) {
                const QRect cell((marker.icon % columns) * flagSize, (marker.icon / columns) * flagSize, flagSize,
                                 flagSize);
                if (m_flagStrip.rect().contains(cell))
                    icon = m_flagStrip.copy(cell);
            }
            addPin(icon, QColor(0xff, 0xcc, 0x33), marker.x, marker.y, z, QString::fromStdString(marker.description));
        }
    }
    // CIP TPassageMapAreaData from 0xDD subtype 6 (TCyclopediaMapStorage).
    if (m_showPassages && m_storage) {
        const auto& staticNpcs = cyclopedia::TCyclopediaMapStaticData::instance().npcs();
        for (const auto& passage : m_storage->passages()) {
            if (passage.coordinate.z != z)
                continue;
            QString tooltip = qtTrId("passage_tooltip");
            for (const auto& npc : staticNpcs) {
                if (npc.coordinate.x == passage.coordinate.x && npc.coordinate.y == passage.coordinate.y
                    && npc.coordinate.z == passage.coordinate.z) {
                    tooltip = npc.name;
                    break;
                }
            }
            addPin(m_passageIcon, QColor(0x66, 0xcc, 0xff), passage.coordinate.x, passage.coordinate.y, z, tooltip);
        }
    }
    m_playerLayer = -1;
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
        const Position position = player->getPosition();
        m_playerField = QPoint(position.x, position.y);
        m_playerLayer = position.z;
        const bool drawn = position.z == z || (z <= GroundLayer && position.z > z && position.z <= GroundLayer && m_tileType != ETileType::LocalMinimap);
        if (position.isValid() && drawn)
            addPin(m_playerIcon, QColor(0x66, 0xff, 0x5e), position.x, position.y, position.z,
                   qtTrId("player_position_tooltip"));
    }
}

QString TMinimapRendererQuickItem::markerTooltipAt(const QPointF& point) const
{
    for (auto it = m_markers.rbegin(); it != m_markers.rend(); ++it) {
        if (it->rect.contains(point))
            return it->tooltip;
    }
    return {};
}

// CIP 0x1010c0f10: area captions fade into subarea captions between the
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
        for (const auto& area : cyclopedia::TCyclopediaMapStaticData::instance().areas()) {
            if (!area.hasLabel)
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
        for (const TExpeditionCaption& pin : m_expeditionCaptions) {
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
    auto* root = oldNode ? static_cast<TRendererNode*>(oldNode) : new TRendererNode;
    root->clear();
    std::unordered_map<qint64, QSGTexture*> used;
    for (const TDraw& draw : m_drawList) {
        for (const TImageDraw& image : draw.images) {
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
            if (image.opacity < 1.0) {
                auto* opacityNode = new QSGOpacityNode;
                opacityNode->setOpacity(image.opacity);
                opacityNode->appendChildNode(node);
                root->appendChildNode(opacityNode);
            } else {
                root->appendChildNode(node);
            }
        }
        for (const TRectDraw& rect : draw.rects) {
            QSGRectangleNode* node = window()->createRectangleNode();
            node->setRect(rect.rect);
            node->setColor(rect.color);
            root->appendChildNode(node);
        }
    }
    deleteTextures(root->textures);
    root->textures = std::move(used);
    return root;
}

}
