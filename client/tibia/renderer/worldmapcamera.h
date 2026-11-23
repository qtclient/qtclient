#pragma once

#include "worldmap/coordinates.h"

#include <QMatrix4x4>
#include <QPoint>
#include <QSize>

#include <cip_gen.h>

namespace tibia {
namespace renderer {

/// A camera anchored at its top-left corner: the subfield coordinate drawn at
/// the view's (0, 0) and a zoom. The Cyclopedia map wraps one
/// (qmlcomponents::TWorldMapCamera).
///
/// A subfield coordinate is drawn where its projection onto layer 0 (one field
/// up and left per layer above it) lands; the matrix maps that projection to a
/// pixel of the view: minus the top-left's projection, times the scale factor.
/// The reference point is a pixel of the view and the subfield coordinate on
/// the top-left's layer under it when it was set; zooming and dragging keep
/// that coordinate under that pixel.
class TWorldMapCamera : public cip::tibia::renderer::TWorldMapCamera
{
    Q_OBJECT
public:
    explicit TWorldMapCamera(QObject* parent = nullptr);

    worldmap::TWorldMapSubfieldCoordinate topLeftSubfieldCoordinate() const { return m_topLeftSubfieldCoordinate; }
    /// Rebuilds the matrix when the coordinate differs.
    void setTopLeftSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate);
    /// setTopLeftSubfieldCoordinate once the event loop runs.
    void setTopLeftSubfieldCoordinateDelayed(const worldmap::TWorldMapSubfieldCoordinate& coordinate);

    float scaleFactor() const { return m_scaleFactor; }
    /// Zooms keeping the reference point's subfield coordinate under it.
    void setScaleFactor(float scaleFactor);

    QPoint referencePoint() const { return m_referencePoint; }
    /// Also remembers the subfield coordinate under the point.
    void setReferencePoint(const QPoint& referencePoint);

    QSize viewportSize() const { return m_viewportSize; }
    /// The first call announces geometryIntialized.
    void setViewportSize(const QSize& viewportSize);

    QMatrix4x4 matrix() const { return m_matrix; }

    /// The pixel back to its projection onto layer 0 (rounded to whole subfields).
    QPoint convertToProjectedSubfield(const QPoint& point) const;
    /// The subfield coordinate on the top-left's layer drawn at the pixel.
    worldmap::TWorldMapSubfieldCoordinate convertToWorldMapSubfieldCoordinate(const QPoint& point) const;
    /// Where the subfield coordinate is drawn (rounded to whole pixels).
    QPoint convertSubfieldCoordinateToPoint(const worldmap::TWorldMapSubfieldCoordinate& coordinate) const;

    /// Moves the top-left so that `coordinate` is drawn at `point`; the
    /// top-left takes the coordinate's layer.
    void moveSubfieldCoordinateToPoint(const worldmap::TWorldMapSubfieldCoordinate& coordinate, const QPoint& point);
    /// Drags the view: the reference point's subfield coordinate moves by the
    /// subfields `distance` pixels span, back under the reference point.
    void moveDistanceRelativeToReferencePoint(const QPoint& distance);
    /// Moves `coordinate` to the middle of the viewport (half its size, rounded).
    void centerAtSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate);
    /// centerAtSubfieldCoordinate once the event loop runs (with the viewport
    /// size of that moment).
    void centerAtSubfieldCoordinateDelayed(const worldmap::TWorldMapSubfieldCoordinate& coordinate);

private:
    /// Rebuilds both matrices from the top-left and the scale factor;
    /// announces cameraViewportChanged when they changed.
    void updateMatrix();

    /// scale(scaleFactor) * translate(-top-left projection).
    QMatrix4x4 m_matrix;
    /// translate(-top-left projection).
    QMatrix4x4 m_translationMatrix;
    worldmap::TWorldMapSubfieldCoordinate m_topLeftSubfieldCoordinate;
    QPoint m_referencePoint;
    worldmap::TWorldMapSubfieldCoordinate m_referenceSubfieldCoordinate;
    float m_scaleFactor = 1.0f;
    QSize m_viewportSize;
    bool m_geometryInitialized = false;
};

}
}
