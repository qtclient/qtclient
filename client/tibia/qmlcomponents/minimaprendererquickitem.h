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

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStorage;
}
}

namespace tibia {
namespace minimap {
class TMinimapTileStorage;
}
}

namespace tibia {
namespace qmlcomponents {

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
    /// after the QML that creates it has set its scale (CIP 
    /// defers it with a zero timer).
    void centerDelayed(int subfieldX, int subfieldY, int layer);
    /// Applies a pending centerDelayed once the viewport has a size.
    void applyPendingCenter();

    void setTopLeftSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v) override;
    /// CIP : keeps the coordinate under the reference point.
    void setScaleFactor(float v) override;
    /// CIP : also remembers the coordinate under the point.
    void setReferencePoint(const QPoint& v) override;
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* coordinateAtPoint(const QPoint& Value) override;
    /// CIP .
    void changeLayerWithIsometricProjection(int NewLayer) override;
    /// CIP .
    void moveDistanceRelativeToReferencePoint(const QPoint& Distance) override;
    void centerAtSubfieldCoordinateDelayed(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pSubfieldCoordinate) override;

Q_SIGNALS:
    /// CIP NOTIFY deepen — qtclient owns emit sites on setScale / place / center.
    void cameraViewportChanged();
    /// CIP renderer::TWorldMapCamera::geometryIntialized (CIP spelling) — first sized viewport.
    void geometryIntialized();

private:
    QPoint subfieldAt(const QPointF& point) const;
    void placeAt(const QPoint& subfield, const QPointF& point);
    void setTopLeft(const QPoint& subfield, int layer);

    QSizeF m_viewport;
    QPoint m_referenceSubfield;
    bool m_hasPendingCenter = false;
    bool m_geometryInitialized = false;
    QPoint m_pendingCenter;
    int m_pendingLayer = 7;
};

/// One area caption (CIP: TAreaCaptionController,).
class TAreaCaptionController final : public cip::tibia::qmlcomponents::TAreaCaptionController
{
    Q_OBJECT
public:
    TAreaCaptionController(int id, bool isArea, QObject* parent);

    void set(const QString& caption, const QColor& color, bool improvedRespawnActive);
    /// CIP : emits clicked with the caption's area.
    void onClicked() override;
    /// QML must not call onClicked (Qt treats that name as a signal handler).
    Q_INVOKABLE void activate();

Q_SIGNALS:
    void clicked(int id, bool isArea);

private:
    int m_id;
    bool m_isArea;
};

/// MinimapRenderer (CIP: TMinimapRendererQuickItem, updatePaintNode
/// ): the Cyclopedia map's satellite / minimap images, the floor
/// separators, highlighted subareas, NPC and player markers and the area
/// captions (AreaCaptionTemplate.qml,).
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
    struct TMissionZoneOverlay
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
    struct TMissionCaption
    {
        int id = 0;
        QString name;
        int x = 0;
        int y = 0;
        int z = 7;
    };
    void setMissionZones(std::vector<TMissionZoneOverlay> zones);
    void setMissionCaptions(std::vector<TMissionCaption> captions);
    /// When false, CIP subarea captions are hidden (packs use catalog mission pins per floor).
    void setShowCyclopediaSubareaCaptions(bool show);
    /// The local minimap tiles LocalMinimap draws (process-wide).
    static void setMinimapTileStorage(minimap::TMinimapTileStorage* storage);
    /// Rebuild flag pins after TMinimapMarkerStorage CRUD.
    static void notifyMarkersChanged();
    /// The tooltip of the topmost NPC / player marker under `point`.
    QString markerTooltipAt(const QPointF& point) const;

Q_SIGNALS:
    /// A caption was clicked (CIP : TAreaData).
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
    QImage missionZoneImage(const TMissionZoneOverlay& zone);
    /// CIP / DAT_1077b32c8: (sin(t_ms/100)*0.15+0.65)*0.75.
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
    std::unordered_map<int, TMissionZoneOverlay> m_missionZones;
    std::vector<TMissionCaption> m_missionCaptions;
    std::vector<TDraw> m_drawList;
    std::map<int, QImage> m_zoneImages;
    std::map<int, QImage> m_missionZoneImages;
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
}
