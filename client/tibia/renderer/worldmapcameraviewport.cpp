#include "renderer/worldmapcameraviewport.h"

#include <algorithm>

namespace tibia::renderer {

TWorldMapCameraViewport::TWorldMapCameraViewport(QObject* parent)
    : cip::tibia::renderer::TWorldMapCameraViewport(parent)
    , m_viewportChangedTimer(nullptr)
{
    m_viewportChangedTimer.setInterval(0);
    m_viewportChangedTimer.setSingleShot(true);
    connect(&m_viewportChangedTimer, &QTimer::timeout, this, &TWorldMapCameraViewport::viewportChanged);
}

void TWorldMapCameraViewport::updateMatrix()
{
    const QPoint origin = m_centerSubfieldCoordinate.projectedToLayer(0);
    m_matrix.setToIdentity();
    float divisor = 2.0f;
    if (m_scaleFactor != 1.0f) {
        m_matrix.scale(m_scaleFactor);
        divisor = m_scaleFactor + m_scaleFactor;
    }
    m_matrix.translate(float(m_size.width()) / divisor + float(-origin.x()),
                       float(m_size.height()) / divisor + float(-origin.y()));
}

void TWorldMapCameraViewport::setScaleFactor(const float scaleFactor)
{
    if (m_scaleFactor == scaleFactor)
        return;
    m_scaleFactor = scaleFactor;
    updateMatrix();
    if (!m_viewportChangedTimer.isActive())
        m_viewportChangedTimer.start();
}

void TWorldMapCameraViewport::setSize(const QSize& size)
{
    const QSize newSize = size.height() < 1 || size.width() < 1 ? QSize(0, 0) : size;
    if (m_size == newSize)
        return;
    m_size = newSize;
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::setCenterSubfieldCoordinate(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    const int layer = std::min(std::max(m_topLayer, coordinate.z), m_bottomLayer);
    if (layer != coordinate.z) {
        translateToLayer(layer);
        return;
    }
    if (m_centerSubfieldCoordinate == coordinate)
        return;
    m_centerSubfieldCoordinate = coordinate;
    updateMatrix();
    if (!m_viewportChangedTimer.isActive())
        m_viewportChangedTimer.start();
}

void TWorldMapCameraViewport::setTopAndBottomLayer(int topLayer, int bottomLayer)
{
    if (topLayer > 15)
        topLayer = 15;
    if (topLayer < 1)
        topLayer = 0;
    if (bottomLayer > 15)
        bottomLayer = 15;
    m_topLayer = topLayer;
    if (bottomLayer < topLayer)
        bottomLayer = topLayer;
    m_bottomLayer = bottomLayer;
    // The centre moves into the shown layers.
    setCenterSubfieldCoordinate(worldmap::TWorldMapSubfieldCoordinate(m_centerSubfieldCoordinate));
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::setAdditionalColumnsAndRows(const int additionalColumnsLeft, const int additionalColumnsRight,
                                                          const int additionalRowsTop, const int additionalRowsBottom)
{
    m_additionalColumnsLeft = additionalColumnsLeft;
    m_additionalColumnsRight = additionalColumnsRight;
    m_additionalRowsTop = additionalRowsTop;
    m_additionalRowsBottom = additionalRowsBottom;
    if (!m_viewportChangedTimer.isActive())
        m_viewportChangedTimer.start();
}

void TWorldMapCameraViewport::translateToLayer(const int newLayer)
{
    worldmap::TWorldMapCoordinate field = m_centerSubfieldCoordinate.toFieldCoordinate();
    field.z = newLayer;
    field = field.shiftedTowardsFloor(m_centerSubfieldCoordinate.z);
    setCenterSubfieldCoordinate(
        worldmap::subfieldOfField(field, worldmap::TWorldMapSubfieldExtent::offsetInField(m_centerSubfieldCoordinate)));
}

void TWorldMapCameraViewport::translateByStretchedPixelCoordinate(const QPoint& stretchedPixelCoordinate)
{
    const QPoint offset = stretchedPixelCoordinate / double(m_scaleFactor);
    setCenterSubfieldCoordinate(m_centerSubfieldCoordinate + worldmap::TWorldMapSubfieldExtent(offset.x(), offset.y(), 0));
}

void TWorldMapCameraViewport::translateBySubfieldOffset(const worldmap::TWorldMapSubfieldExtent& subfieldOffset)
{
    setCenterSubfieldCoordinate(m_centerSubfieldCoordinate
                                + worldmap::TWorldMapSubfieldExtent(subfieldOffset.width, subfieldOffset.height, 0));
}

void TWorldMapCameraViewport::moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(
    const worldmap::TWorldMapSubfieldCoordinate& coordinate, const QPoint& pixelCoordinate)
{
    const worldmap::TWorldMapSubfieldCoordinate underPixel =
        convertToWorldMapSubfieldCoordinateForLayer(pixelCoordinate, m_centerSubfieldCoordinate.z);
    const worldmap::TWorldMapSubfieldExtent offset = coordinate - underPixel;
    setCenterSubfieldCoordinate(worldmap::TWorldMapSubfieldCoordinate(m_centerSubfieldCoordinate) + offset);
}

void TWorldMapCameraViewport::zoomAtPoint(const float scaleFactor, const QPoint& stretchedPixelCoordinate)
{
    const worldmap::TWorldMapSubfieldCoordinate underPixel =
        convertToWorldMapSubfieldCoordinateForLayer(stretchedPixelCoordinate, m_centerSubfieldCoordinate.z);
    setScaleFactor(scaleFactor);
    // Where that subfield is drawn now, back under the pixel.
    const QPoint drawnAt = convertSubfieldCoordinateToStretchedPixelCoordinate(underPixel);
    const QPoint offset = (drawnAt - stretchedPixelCoordinate) / double(m_scaleFactor);
    setCenterSubfieldCoordinate(m_centerSubfieldCoordinate + worldmap::TWorldMapSubfieldExtent(offset.x(), offset.y(), 0));
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapCameraViewport::convertToWorldMapSubfieldCoordinate(
    const QPoint& stretchedPixelCoordinate) const
{
    return convertToWorldMapSubfieldCoordinateForLayer(stretchedPixelCoordinate, m_centerSubfieldCoordinate.z);
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapCameraViewport::convertToWorldMapSubfieldCoordinateForLayer(
    const QPoint& stretchedPixelCoordinate, const int layer) const
{
    // The pixel back to its projection onto layer 0, then up to `layer`.
    return worldmap::TWorldMapSubfieldCoordinate::fromProjection(m_matrix.inverted().map(stretchedPixelCoordinate), layer);
}

QPoint TWorldMapCameraViewport::convertSubfieldCoordinateToStretchedPixelCoordinate(
    const worldmap::TWorldMapSubfieldCoordinate& coordinate) const
{
    return m_matrix.map(coordinate.projectedToLayer(0));
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapCameraViewport::centerLayerTopLeftSubfieldCoordinate() const
{
    return convertToWorldMapSubfieldCoordinateForLayer(QPoint(0, 0), m_centerSubfieldCoordinate.z);
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapCameraViewport::centerLayerBottomRightSubfieldCoordinate() const
{
    return convertToWorldMapSubfieldCoordinateForLayer(QPoint(m_size.width(), m_size.height()), m_centerSubfieldCoordinate.z);
}

worldmap::TWorldMapExtent TWorldMapCameraViewport::volumeExtent() const
{
    const worldmap::TWorldMapCoordinate topLeft =
        convertToWorldMapSubfieldCoordinateForLayer(QPoint(0, 0), m_topLayer).toFieldCoordinate();
    const worldmap::TWorldMapSubfieldCoordinate bottomRightSubfield =
        convertToWorldMapSubfieldCoordinateForLayer(QPoint(m_size.width(), m_size.height()), m_topLayer);
    worldmap::TWorldMapCoordinate bottomRight = bottomRightSubfield.toFieldCoordinate();

    const worldmap::TWorldMapCoordinate first =
        topLeft - worldmap::TWorldMapExtent(m_additionalColumnsLeft, m_additionalRowsTop, 0);
    bottomRight = bottomRight + worldmap::TWorldMapExtent(m_additionalColumnsRight, m_additionalRowsBottom, 0);
    if (worldmap::TWorldMapSubfieldExtent::offsetInField(bottomRightSubfield).width > 0)
        bottomRight = bottomRight + worldmap::TWorldMapExtent(1, 0, 0);
    if (worldmap::TWorldMapSubfieldExtent::offsetInField(bottomRightSubfield).height > 0)
        bottomRight = bottomRight + worldmap::TWorldMapExtent(0, 1, 0);

    worldmap::TWorldMapExtent extent = bottomRight - first;
    extent.depth = m_bottomLayer - m_topLayer + 1;
    return extent;
}

worldmap::TShearedWorldMapCuboid TWorldMapCameraViewport::volume() const
{
    worldmap::TWorldMapCoordinate origin = centerLayerTopLeftSubfieldCoordinate().toFieldCoordinate();
    origin.z = m_topLayer;
    origin = origin - worldmap::TWorldMapExtent(m_additionalColumnsLeft, m_additionalRowsTop, 0);
    return worldmap::TShearedWorldMapCuboid(origin, volumeExtent(), m_centerSubfieldCoordinate.z);
}

QPoint TWorldMapCameraViewport::centerLayerStretchedPixelOffset() const
{
    const worldmap::TShearedWorldMapCuboid shownVolume = volume();
    // The volume's origin seen from its shear layer, and back.
    const worldmap::TWorldMapCoordinate sheared = shownVolume.origin.shiftedTowardsFloor(shownVolume.shearFloor);
    const worldmap::TWorldMapCoordinate field = sheared.shiftedAwayFromFloor(volume().shearFloor);
    return convertSubfieldCoordinateToStretchedPixelCoordinate(worldmap::TWorldMapSubfieldCoordinate::fromField(field, 0, 0));
}

}
