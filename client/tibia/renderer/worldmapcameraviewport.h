#pragma once

#include "worldmap/coordinates.h"

#include <QMatrix4x4>
#include <QPoint>
#include <QSize>
#include <QTimer>

#include <cip_gen.h>

namespace tibia {
namespace renderer {

/// The camera of a world map view: the subfield coordinate shown in the middle
/// of the view, the zoom, the view's size and the layers it shows.
///
/// A subfield coordinate is drawn where its projection onto layer 0 (one field
/// up and left per layer above it) lands; the matrix maps that projection to a
/// stretched pixel of the view, the centre's projection landing in the middle.
/// Changes of the centre, the zoom or the extra rows and columns announce
/// viewportChanged once, on the next turn of the event loop.
class TWorldMapCameraViewport : public cip::tibia::renderer::TWorldMapCameraViewport
{
    Q_OBJECT
public:
    explicit TWorldMapCameraViewport(QObject* parent = nullptr);

    float scaleFactor() const { return m_scaleFactor; }
    void setScaleFactor(float scaleFactor);
    QSize size() const { return m_size; }
    /// A size without area counts as 0x0. Announced at once; the matrix keeps
    /// the old size until the centre or the zoom changes.
    void setSize(const QSize& size);
    QMatrix4x4 matrix() const { return m_matrix; }

    worldmap::TWorldMapSubfieldCoordinate centerSubfieldCoordinate() const { return m_centerSubfieldCoordinate; }
    /// A layer outside the shown layers moves the current centre to the nearest
    /// shown layer instead (translateToLayer).
    void setCenterSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate);

    int topLayer() const { return m_topLayer; }
    int bottomLayer() const { return m_bottomLayer; }
    /// Both layers lie in 0..15 and the bottom one is not above the top one.
    void setTopAndBottomLayer(int topLayer, int bottomLayer);
    /// Fields added to the volume beyond each edge of the view.
    void setAdditionalColumnsAndRows(int additionalColumnsLeft, int additionalColumnsRight, int additionalRowsTop,
                                     int additionalRowsBottom);

    /// The centre's field as seen from `newLayer` (one field down-right per
    /// layer up), keeping the position inside the field.
    void translateToLayer(int newLayer);
    void translateByStretchedPixelCoordinate(const QPoint& stretchedPixelCoordinate);
    void translateBySubfieldOffset(const worldmap::TWorldMapSubfieldExtent& subfieldOffset);
    /// Moves the centre so that `coordinate` lands on `pixelCoordinate`.
    void moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate,
                                                                   const QPoint& pixelCoordinate);
    /// Zooms keeping the subfield under `stretchedPixelCoordinate` in place.
    void zoomAtPoint(float scaleFactor, const QPoint& stretchedPixelCoordinate);

    /// The subfield coordinate on the centre's layer drawn at the pixel.
    worldmap::TWorldMapSubfieldCoordinate convertToWorldMapSubfieldCoordinate(const QPoint& stretchedPixelCoordinate) const;
    worldmap::TWorldMapSubfieldCoordinate convertToWorldMapSubfieldCoordinateForLayer(const QPoint& stretchedPixelCoordinate,
                                                                                      int layer) const;
    QPoint convertSubfieldCoordinateToStretchedPixelCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate) const;

    /// The subfield coordinates on the centre's layer at the view's corners.
    worldmap::TWorldMapSubfieldCoordinate centerLayerTopLeftSubfieldCoordinate() const;
    worldmap::TWorldMapSubfieldCoordinate centerLayerBottomRightSubfieldCoordinate() const;
    /// The fields the view shows from the top layer to the bottom layer, sheared
    /// at the centre's layer, with the additional rows and columns.
    worldmap::TShearedWorldMapCuboid volume() const;
    /// Where the top-left field of the volume is drawn.
    QPoint centerLayerStretchedPixelOffset() const;

public:
    /// The volume's size: the fields between the view's corners on the top
    /// layer (a corner inside a field takes that field), widened by the
    /// additional rows and columns, over the shown layers.
    worldmap::TWorldMapExtent volumeExtent() const;

private:
    void updateMatrix();

    /// Zero-interval single shot: collects the changes of one turn into one
    /// viewportChanged.
    QTimer m_viewportChangedTimer;
    float m_scaleFactor = 1.0f;
    QSize m_size;
    QMatrix4x4 m_matrix;
    worldmap::TWorldMapSubfieldCoordinate m_centerSubfieldCoordinate;
    int m_topLayer = 0;
    int m_bottomLayer = 15;
    int m_additionalColumnsLeft = 0;
    int m_additionalColumnsRight = 0;
    int m_additionalRowsTop = 0;
    int m_additionalRowsBottom = 0;
};

}
}
