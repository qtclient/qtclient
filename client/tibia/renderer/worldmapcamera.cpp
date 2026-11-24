#include "renderer/worldmapcamera.h"

#include <QTimer>

#include <algorithm>
#include <climits>
#include <cmath>

namespace tibia::renderer {

namespace {

/// Half of a viewport extent, rounded half away from zero and kept in the int range.
int halfRounded(const int extent)
{
    const double half = extent * 0.5;
    const double rounded = half + std::copysign(0.5, half);
    return static_cast<int>(std::max(std::min(rounded, double(INT_MAX)), double(INT_MIN)));
}

}

TWorldMapCamera::TWorldMapCamera(QObject* parent)
    : cip::tibia::renderer::TWorldMapCamera(parent)
{
}

void TWorldMapCamera::setTopLeftSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    if (m_topLeftSubfieldCoordinate == coordinate)
        return;
    m_topLeftSubfieldCoordinate = coordinate;
    updateMatrix();
}

void TWorldMapCamera::setTopLeftSubfieldCoordinateDelayed(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    QTimer::singleShot(0, Qt::PreciseTimer, this, [this, coordinate] {
        if (m_topLeftSubfieldCoordinate == coordinate)
            return;
        m_topLeftSubfieldCoordinate = coordinate;
        updateMatrix();
    });
}

void TWorldMapCamera::updateMatrix()
{
    const QPoint origin = m_topLeftSubfieldCoordinate.projectedToLayer(0);
    const float x = static_cast<float>(-origin.x());
    const float y = static_cast<float>(-origin.y());
    QMatrix4x4 translationMatrix;
    translationMatrix.translate(x, y);
    QMatrix4x4 matrix;
    if (m_scaleFactor != 1.0f)
        matrix.scale(m_scaleFactor);
    matrix.translate(x, y);
    if (m_translationMatrix == translationMatrix && m_matrix == matrix)
        return;
    m_translationMatrix = translationMatrix;
    m_matrix = matrix;
    Q_EMIT cameraViewportChanged();
}

void TWorldMapCamera::setScaleFactor(const float scaleFactor)
{
    if (m_scaleFactor == scaleFactor)
        return;
    const worldmap::TWorldMapSubfieldCoordinate underReferencePoint = convertToWorldMapSubfieldCoordinate(m_referencePoint);
    m_scaleFactor = scaleFactor;
    updateMatrix();
    moveSubfieldCoordinateToPoint(underReferencePoint, m_referencePoint);
}

void TWorldMapCamera::setReferencePoint(const QPoint& referencePoint)
{
    m_referencePoint = referencePoint;
    m_referenceSubfieldCoordinate = convertToWorldMapSubfieldCoordinate(m_referencePoint);
}

void TWorldMapCamera::setViewportSize(const QSize& viewportSize)
{
    m_viewportSize = viewportSize;
    if (m_geometryInitialized)
        return;
    m_geometryInitialized = true;
    Q_EMIT geometryIntialized();
}

QPoint TWorldMapCamera::convertToProjectedSubfield(const QPoint& point) const
{
    return m_matrix.inverted().map(point);
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapCamera::convertToWorldMapSubfieldCoordinate(const QPoint& point) const
{
    return worldmap::TWorldMapSubfieldCoordinate::fromProjection(convertToProjectedSubfield(point), m_topLeftSubfieldCoordinate.z);
}

QPoint TWorldMapCamera::convertSubfieldCoordinateToPoint(const worldmap::TWorldMapSubfieldCoordinate& coordinate) const
{
    return m_matrix.map(coordinate.projectedToLayer(0));
}

void TWorldMapCamera::moveSubfieldCoordinateToPoint(const worldmap::TWorldMapSubfieldCoordinate& coordinate, const QPoint& point)
{
    // How far the subfield drawn at the point lies from the top-left; the
    // top-left moves to the same distance from the coordinate.
    const worldmap::TWorldMapSubfieldExtent offset = convertToWorldMapSubfieldCoordinate(point) - m_topLeftSubfieldCoordinate;
    const worldmap::TWorldMapSubfieldCoordinate topLeft(coordinate.subfieldX - offset.width, coordinate.subfieldY - offset.height,
                                                        coordinate.z - offset.depth);
    if (m_topLeftSubfieldCoordinate == topLeft)
        return;
    m_topLeftSubfieldCoordinate = topLeft;
    updateMatrix();
}

void TWorldMapCamera::moveDistanceRelativeToReferencePoint(const QPoint& distance)
{
    const worldmap::TWorldMapSubfieldExtent offset = convertToWorldMapSubfieldCoordinate(distance) - m_topLeftSubfieldCoordinate;
    const worldmap::TWorldMapSubfieldCoordinate moved(m_referenceSubfieldCoordinate.subfieldX + offset.width,
                                                      m_referenceSubfieldCoordinate.subfieldY + offset.height,
                                                      m_referenceSubfieldCoordinate.z);
    moveSubfieldCoordinateToPoint(moved, m_referencePoint);
}

void TWorldMapCamera::centerAtSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    moveSubfieldCoordinateToPoint(coordinate, QPoint(halfRounded(m_viewportSize.width()), halfRounded(m_viewportSize.height())));
}

void TWorldMapCamera::centerAtSubfieldCoordinateDelayed(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    QTimer::singleShot(0, Qt::PreciseTimer, this, [this, coordinate] { centerAtSubfieldCoordinate(coordinate); });
}

worldmap::TWorldMapCuboid TWorldMapCamera::visibleArea() const
{
    const QPoint farCorner = convertToProjectedSubfield(QPoint(m_viewportSize.width(), m_viewportSize.height()));
    const worldmap::TWorldMapSubfieldCoordinate bottomRight =
        worldmap::TWorldMapSubfieldCoordinate::fromProjection(farCorner, m_topLeftSubfieldCoordinate.z, 0);
    const worldmap::TWorldMapCoordinate first = m_topLeftSubfieldCoordinate.toFieldCoordinate();
    const worldmap::TWorldMapCoordinate last = bottomRight.toFieldCoordinate();
    return worldmap::TWorldMapCuboid(first, last.x - first.x, last.y - first.y);
}

worldmap::TWorldMapCuboid TWorldMapCamera::visibleArea(const int layer) const
{
    return visibleArea().floorArea(layer);
}

QRect TWorldMapCamera::convertCuboidToRectangle(const worldmap::TWorldMapCuboid& cuboid) const
{
    using worldmap::TWorldMapSubfieldCoordinate;
    const QPoint topLeft = convertSubfieldCoordinateToPoint(TWorldMapSubfieldCoordinate::fromField(cuboid.getOrigin(), 0, 0));
    const QPoint end = convertSubfieldCoordinateToPoint(TWorldMapSubfieldCoordinate::fromField(cuboid.getEnd(), 0, 0));
    return QRect(topLeft, end - QPoint(1, 1));
}

QRect TWorldMapCamera::projectedSubfieldRectangle(const worldmap::TWorldMapCuboid& cuboid) const
{
    const worldmap::TWorldMapCoordinate& origin = cuboid.origin;
    const int left = (origin.z + origin.x) * 32;
    const int top = (origin.z + origin.y) * 32;
    const worldmap::TWorldMapExtent extent = cuboid.getExtent();
    return QRect(QPoint(left, top), QPoint(left + extent.width * 32 - 1, top + extent.height * 32 - 1));
}

}
