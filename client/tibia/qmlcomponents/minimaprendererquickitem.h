#pragma once

#include <cip_gen.h>

#include <QImage>
#include <QPointer>
#include <QTimer>

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

class QQmlComponent;

namespace tibia::cyclopedia {
class TCyclopediaMapStorage;
}

namespace tibia::minimap {
class TMinimapTileStorage;
}

namespace tibia::qmlcomponents {

/// The Cyclopedia map's camera (CIP: TWorldMapCameraQmlType): the top-left
/// subfield coordinate (field * 32) of the viewport, its layer and the scale.
/// A field F on layer L shows at ((F + L) * 32 - (topLeft + z * 32)) * scale,
/// so the floors above the viewed one shift up and left.
class TWorldMapCamera final : public cip::tibia::qmlcomponents::TWorldMapCameraQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCamera(QObject* parent = nullptr);

    QPoint topLeft() const;
    int layer() const;
    void setViewportSize(const QSizeF& size);
    /// The item position of field (x, y) on `layer`'s top-left corner.
    QPointF pixelOf(int x, int y, int layer) const;
    /// The field under `point` on the viewed layer.
    QPoint fieldAt(const QPointF& point) const;
    /// Centers subfield (x, y) of `layer` at the renderer's next polish, so
    /// after the QML that creates it has set its scale (CIP 0x100f77030
    /// defers it with a zero timer).
    void centerDelayed(int subfieldX, int subfieldY, int layer);
    /// Applies a pending centerDelayed once the viewport has a size.
    void applyPendingCenter();

    void setTopLeftSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v) override;
    /// CIP 0x100f76730: keeps the coordinate under the reference point.
    void setScaleFactor(float v) override;
    /// CIP 0x100f767d0: also remembers the coordinate under the point.
    void setReferencePoint(const QPoint& v) override;
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* coordinateAtPoint(const QPoint& Value) override;
    /// CIP 0x100f76a10.
    void changeLayerWithIsometricProjection(int NewLayer) override;
    /// CIP 0x100f76900.
    void moveDistanceRelativeToReferencePoint(const QPoint& Distance) override;
    void centerAtSubfieldCoordinateDelayed(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pSubfieldCoordinate) override;

private:
    QPoint subfieldAt(const QPointF& point) const;
    void placeAt(const QPoint& subfield, const QPointF& point);
    void setTopLeft(const QPoint& subfield, int layer);

    QSizeF m_viewport;
    QPoint m_referenceSubfield;
    bool m_hasPendingCenter = false;
    QPoint m_pendingCenter;
    int m_pendingLayer = 7;
};

/// One area caption (CIP: TAreaCaptionController, 0x1010c1990).
class TAreaCaptionController final : public cip::tibia::qmlcomponents::TAreaCaptionController
{
    Q_OBJECT
public:
    TAreaCaptionController(int id, bool isArea, QObject* parent);

    void set(const QString& caption, const QColor& color, bool improvedRespawnActive);
    /// CIP 0x1010c1d50: emits clicked with the caption's area.
    void onClicked() override;
    /// QML must not call onClicked() (Qt treats that name as a signal handler).
    Q_INVOKABLE void activate();

Q_SIGNALS:
    void clicked(int id, bool isArea);

private:
    int m_id;
    bool m_isArea;
};

/// MinimapRenderer (CIP: TMinimapRendererQuickItem, updatePaintNode
/// 0x1010c0b00): the Cyclopedia map's satellite / minimap images, the floor
/// separators, highlighted subareas, NPC and player markers and the area
/// captions (AreaCaptionTemplate.qml, 0x1010c0f10).
class TMinimapRendererQuickItem : public cip::tibia::qmlcomponents::TMinimapRendererQuickItem
{
    Q_OBJECT
public:
    explicit TMinimapRendererQuickItem(QQuickItem* parent = nullptr);
    ~TMinimapRendererQuickItem() override;

    TWorldMapCamera* worldCamera() const;
    /// The progress the caption colours come from; null shows them white.
    void setStorage(cyclopedia::TCyclopediaMapStorage* storage);
    /// The subareas filled with the zone colour (CIP: the selection's set).
    void setHighlightedSubareas(const std::set<int>& subareas);
    /// Catalog hunt masks (overviewFile + boundary), including underground floors.
    struct TExpeditionZoneOverlay
    {
        int id = 0;
        QString name;
        QString file;
        int x = 0;
        int y = 0;
        int z = 7;
        int w = 0;
        int h = 0;
    };
    /// Caption pin for a hunt that has no CIP AreaData label (generated floors).
    struct TExpeditionCaption
    {
        int id = 0;
        QString name;
        int x = 0;
        int y = 0;
        int z = 7;
    };
    void setExpeditionZones(std::vector<TExpeditionZoneOverlay> zones);
    void setExpeditionCaptions(std::vector<TExpeditionCaption> captions);
    /// When false, CIP subarea captions are hidden (expedition uses catalog pins per floor).
    void setShowCyclopediaSubareaCaptions(bool show);
    /// The local minimap tiles LocalMinimap draws (process-wide).
    static void setMinimapTileStorage(minimap::TMinimapTileStorage* storage);
    /// Rebuild flag pins after TMinimapMarkerStorage CRUD.
    static void notifyMarkersChanged();
    /// The tooltip of the topmost NPC / player marker under `point`.
    QString markerTooltipAt(const QPointF& point) const;

Q_SIGNALS:
    /// A caption was clicked (CIP 0x101083240: TAreaData).
    void areaClicked(int id, bool isArea);

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    struct TImageDraw
    {
        QImage image;
        QRectF target;
        QRectF source;
        bool smooth = false;
        /// CIP DAT_1077b32c8: pulsed for selected-subarea zones; else 1.
        qreal opacity = 1.0;
    };
    struct TRectDraw
    {
        QRectF rect;
        QColor color;
    };
    /// One layer's images then its overlays, in drawing order.
    struct TDraw
    {
        std::vector<TImageDraw> images;
        std::vector<TRectDraw> rects;
    };

    void scheduleUpdate();
    void syncZoneBlinkTimer();
    void buildDrawList();
    void addTiles(TDraw& draw, int layer);
    void addLocalMinimap(TDraw& draw, int layer);
    void addZones(TDraw& draw, int layer);
    void addMarkers(TDraw& draw);
    void ensureMarkerIcons();
    void updateCaptions();
    QImage zoneImage(int subareaId, const QImage& mask);
    QImage expeditionZoneImage(const TExpeditionZoneOverlay& zone);
    /// CIP FUN_100f7ffb0 / DAT_1077b32c8: (sin(t_ms/100)*0.15+0.65)*0.75.
    static float zoneHighlightOpacity();

    struct TMarker
    {
        QRectF rect;
        QString tooltip;
    };
    struct TCaption
    {
        QQuickItem* item = nullptr;
        TAreaCaptionController* controller = nullptr;
    };

    TWorldMapCamera* m_worldCamera;
    QPointer<cyclopedia::TCyclopediaMapStorage> m_storage;
    bool m_showCyclopediaSubareaCaptions = true;
    std::set<int> m_highlighted;
    std::unordered_map<int, TExpeditionZoneOverlay> m_expeditionZones;
    std::vector<TExpeditionCaption> m_expeditionCaptions;
    std::vector<TDraw> m_drawList;
    std::map<int, QImage> m_zoneImages;
    std::map<int, QImage> m_expeditionZoneImages;
    std::map<qint64, QImage> m_localTiles;
    QImage m_npcIcon;
    QImage m_playerIcon;
    QImage m_houseIcon;
    QImage m_passageIcon;
    QImage m_flagStrip;
    QQmlComponent* m_captionComponent = nullptr;
    std::map<int, TCaption> m_captions;
    std::vector<TMarker> m_markers;
    QTimer m_playerTimer;
    QTimer m_zoneBlinkTimer;
    QPoint m_playerField;
    int m_playerLayer = -1;
};

}
