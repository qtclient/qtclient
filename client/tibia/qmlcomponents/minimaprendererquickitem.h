#pragma once

#include <cip_gen.h>
#include <client/position.h>
#include "worldmap/coordinates.h"

#include <QImage>
#include <QPointer>
#include <QRect>
#include <QTimer>

#include <map>
#include <memory>
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
class TMinimapRenderInfoStorage;
class TMinimapRendererScenegraphDrawCommandProvider;
class TMinimapTileStorage;
}
namespace sprites {
class TResourceSpritesManager;
}
}

namespace tibia {
namespace renderer {
class TWorldMapCamera;
}
}

namespace tibia {
namespace qmlcomponents {

/// The Cyclopedia map's camera handed to QML (CIP: TWorldMapCameraQmlType): a
/// renderer::TWorldMapCamera and the QML object holding its top-left subfield
/// coordinate. A field F on layer L shows at
/// ((F + L) * 32 - (topLeft + z * 32)) * scale, so the floors above the viewed
/// one shift up and left.
class TWorldMapCamera final : public cip::tibia::qmlcomponents::TWorldMapCameraQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCamera(QObject* parent = nullptr);

    renderer::TWorldMapCamera* camera() const { return m_camera; }
    QPoint topLeft() const;
    int layer() const;
    void setViewportSize(const QSizeF& size);
    /// The item position of field (x, y) on `layer`'s top-left corner.
    QPointF pixelOf(int x, int y, int layer) const;
    /// The field under `point` on the viewed layer.
    QPoint fieldAt(const QPointF& point) const;
    /// Centers subfield (x, y) of `layer` at the renderer's next polish once
    /// the viewport has a size (host: the Cyclopedia controller and the pack
    /// center before the item is laid out).
    void centerDelayed(int subfieldX, int subfieldY, int layer);
    /// Puts subfield (x, y) of `layer` at the top-left once the event loop
    /// runs (the Cyclopedia's back button restoring the map).
    void setTopLeftDelayed(int subfieldX, int subfieldY, int layer);
    /// Applies a pending centerDelayed once the viewport has a size.
    void applyPendingCenter();

    /// Copies the coordinate into the top-left object (onTopLeftCoordinateChanged follows).
    void setTopLeftSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v) override;
    float scaleFactor() const override;
    void setScaleFactor(float v) override;
    QPoint referencePoint() const override;
    void setReferencePoint(const QPoint& v) override;
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* coordinateAtPoint(const QPoint& Value) override;
    void changeLayerWithIsometricProjection(int NewLayer) override;
    void moveDistanceRelativeToReferencePoint(const QPoint& Distance) override;
    void centerAtSubfieldCoordinateDelayed(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pSubfieldCoordinate) override;

public Q_SLOTS:
    /// The top-left object changed: moves the camera there.
    void onTopLeftCoordinateChanged();

private:
    /// Copies the camera's top-left into the top-left object.
    void syncTopLeftSubfieldCoordinate();

    renderer::TWorldMapCamera* m_camera;
    bool m_hasPendingCenter = false;
    worldmap::TWorldMapSubfieldCoordinate m_pendingCenter;
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
    /// Hovering a caption does nothing.
    void onHovered() override {}
    /// QML must not call onClicked (Qt treats that name as a signal handler).
    Q_INVOKABLE void activate();

Q_SIGNALS:
    void clicked(int id, bool isArea);

private:
    int m_id;
    bool m_isArea;
};

/// MinimapRenderer (CIP: TMinimapRendererQuickItem): draws the tree of draw
/// commands its TMinimapRendererScenegraphDrawCommandProvider builds from a
/// TMinimapRenderInfoStorage (map images, floor separators, highlighted
/// subareas, markers), plus the area captions (AreaCaptionTemplate.qml).
/// Host: the pack's mission zone overlays are drawn above the tree, and the
/// marker rectangles under the mouse are kept for tooltips and clicks.
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
    /// The field of the selected passage (0,0,0 for none); its marker is drawn
    /// highlighted.
    void setHighlightedCoordinate(const Position& coordinate);
    /// The sprite caches were cleared: the next polish drops the images made
    /// from map files and builds them again.
    void requestRebuild();
    /// Catalog hunt masks (overviewFile + boundary), including underground floors.
    struct TMissionZoneOverlay
    {
        int id = 0;
        QString name;
        QString file;
        QRect fields; // map fields the mask covers
        int z = 7;
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

    /// A marker drawn on the map (CIP: the TWorldMapAreaData alternatives).
    struct TMarkerHit
    {
        enum EKind
        {
            NPCMarker = 0,
            MinimapMarker = 4,
            Passage = 6,
            Viewpoint = 7,
            ActiveRaid = 8,
            PlayerPosition = 9,
            HouseMarker = 10,
        };
        int kind = NPCMarker;
        Position coordinate{ 0, 0, 0 };
        quint32 houseId = 0;
    };
    /// The markers under `point`, the one drawn on top first.
    std::vector<TMarkerHit> markersAt(const QPointF& point) const;

Q_SIGNALS:
    /// A caption was clicked (CIP : TAreaData).
    void areaClicked(int id, bool isArea);

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

public Q_SLOTS:
    void onCameraViewportChanged();
    void onRenderInfosChanged();
    void onRescalingTimerTimeout();

private:
    /// A pack mission zone image drawn over the tree.
    struct TImageDraw
    {
        QImage image;
        QRectF target;
        QRectF source;
        bool smooth = false;
        /// Pulsed like the subarea overlays.
        qreal opacity = 1.0;
    };

    void scheduleUpdate();
    /// Copies the renderer's settings into the render infos and the provider;
    /// the draw commands are rebuilt when one changed.
    void updateRenderInfoSettings();
    /// Rebuilds the draw commands for the item's size, then the captions.
    void updateDrawCommands();
    void addMissionZones(std::vector<TImageDraw>& draws);
    /// The marker rectangles under the mouse (host: tooltips and clicks).
    void updateMarkerHits();
    void ensureMarkerIcons();
    void updateCaptions();
    QImage zoneImage(int subareaId, const QImage& mask);
    QImage missionZoneImage(const TMissionZoneOverlay& zone);

    struct TMarker
    {
        QRectF rect;
        QString tooltip;
        TMarkerHit hit;
    };
    struct TCaption
    {
        QQuickItem* item = nullptr;
        TAreaCaptionController* controller = nullptr;
    };

    TWorldMapCamera* m_worldCamera;
    std::shared_ptr<minimap::TMinimapRendererScenegraphDrawCommandProvider> m_drawCommandProvider;
    /// The draw commands are rebuilt at the next polish.
    bool m_drawCommandsOutdated = true;
    bool m_rescaling = false;
    QTimer m_rescalingTimer;
    std::shared_ptr<minimap::TMinimapRenderInfoStorage> m_renderInfoStorage;
    std::shared_ptr<sprites::TResourceSpritesManager> m_resourceSpritesManager;
    QPointer<cyclopedia::TCyclopediaMapStorage> m_storage;
    bool m_showCyclopediaSubareaCaptions = true;
    std::set<int> m_highlighted;
    Position m_highlightedCoordinate{ 0, 0, 0 };
    bool m_rebuildRequested = false;
    std::unordered_map<int, TMissionZoneOverlay> m_missionZones;
    std::vector<TMissionCaption> m_missionCaptions;
    std::vector<TImageDraw> m_missionZoneDraws;
    std::map<int, QImage> m_zoneImages;
    std::map<int, QImage> m_missionZoneImages;
    QImage m_npcIcon;
    QImage m_playerIcon;
    QImage m_houseIcon;
    QImage m_passageIcon;
    QImage m_passageSelectedIcon;
    QImage m_raidIcon;
    QImage m_flagStrip;
    QQmlComponent* m_captionComponent = nullptr;
    std::map<int, TCaption> m_captions;
    std::vector<TMarker> m_markers;
    QTimer m_playerTimer;
    QPoint m_playerField;
    int m_playerLayer = -1;
};

}
}
