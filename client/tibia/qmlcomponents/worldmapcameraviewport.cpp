#include "worldmapcameraviewport.h"

#include <algorithm>

namespace tibia::qmlcomponents {

TWorldMapCameraViewport::TWorldMapCameraViewport(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType(parent)
{
    m_centerSubfieldCoordinate = new TWorldMapSubfieldCoordinateQmlType(this);
    m_centerLayerTopLeftSubfieldCoordinate = new TWorldMapSubfieldCoordinateQmlType(this);
    m_centerLayerBottomRightSubfieldCoordinate = new TWorldMapSubfieldCoordinateQmlType(this);
    m_cuboid = new TWorldMapCuboidQmlType(this);
    m_volume = m_cuboid;
    m_scaleFactor = 0.5f;
    m_topLayer = 7;
    m_bottomLayer = 7;
    m_centerSubfieldCoordinate->setZ(7);
    rebuildCorners();
}

void TWorldMapCameraViewport::setScaleFactor(const float v)
{
    if (m_scaleFactor == v)
        return;
    m_scaleFactor = v;
    rebuildCorners();
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::setSize(const QSize& v)
{
    if (m_size == v)
        return;
    m_size = v;
    rebuildCorners();
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::setCenterSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v)
{
    if (!v || v == m_centerSubfieldCoordinate)
        return;
    applyCenter(v->subfieldX(), v->subfieldY(), v->z());
}

void TWorldMapCameraViewport::setTopAndBottomLayer(const int TopLayer, const int BottomLayer)
{
    m_topLayer = TopLayer;
    m_bottomLayer = BottomLayer;
    rebuildVolume();
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::translateToLayer(const int NewLayer)
{
    applyCenter(m_centerSubfieldCoordinate->subfieldX(), m_centerSubfieldCoordinate->subfieldY(), NewLayer);
}

void TWorldMapCameraViewport::translateByStretchedPixelCoordinate(const QPoint& StretchedPixelCoordinate)
{
    if (m_scaleFactor <= 0)
        return;
    const int dx = int(StretchedPixelCoordinate.x() / m_scaleFactor);
    const int dy = int(StretchedPixelCoordinate.y() / m_scaleFactor);
    applyCenter(m_centerSubfieldCoordinate->subfieldX() - dx, m_centerSubfieldCoordinate->subfieldY() - dy,
                m_centerSubfieldCoordinate->z());
}

void TWorldMapCameraViewport::translateBySubfieldOffset(const QPoint& SubfieldOffset)
{
    applyCenter(m_centerSubfieldCoordinate->subfieldX() + SubfieldOffset.x(),
                m_centerSubfieldCoordinate->subfieldY() + SubfieldOffset.y(), m_centerSubfieldCoordinate->z());
}

void TWorldMapCameraViewport::moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate, const QPoint& PixelCoordinate)
{
    if (!WorldMapSubfieldCoordinate || m_scaleFactor <= 0 || m_size.isEmpty())
        return;
    const int centerX = WorldMapSubfieldCoordinate->subfieldX() - int((PixelCoordinate.x() - m_size.width() * 0.5) / m_scaleFactor);
    const int centerY = WorldMapSubfieldCoordinate->subfieldY() - int((PixelCoordinate.y() - m_size.height() * 0.5) / m_scaleFactor);
    applyCenter(centerX, centerY, WorldMapSubfieldCoordinate->z());
}

void TWorldMapCameraViewport::zoomAtPoint(const float ScaleFactor, const QPoint& StretchedPixelCoordinate)
{
    auto* under = convertToWorldMapSubfieldCoordinate(StretchedPixelCoordinate);
    setScaleFactor(ScaleFactor);
    if (under)
        moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(under, StretchedPixelCoordinate);
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* TWorldMapCameraViewport::convertToWorldMapSubfieldCoordinate(
    const QPoint& StretchedPixelCoordinate)
{
    return convertToWorldMapSubfieldCoordinateForLayer(StretchedPixelCoordinate, m_centerSubfieldCoordinate->z());
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* TWorldMapCameraViewport::convertToWorldMapSubfieldCoordinateForLayer(
    const QPoint& StretchedPixelCoordinate, const int Layer)
{
    auto* coordinate = new TWorldMapSubfieldCoordinateQmlType;
    if (m_scaleFactor <= 0 || m_size.isEmpty()) {
        coordinate->setSubfieldX(m_centerSubfieldCoordinate->subfieldX());
        coordinate->setSubfieldY(m_centerSubfieldCoordinate->subfieldY());
        coordinate->setZ(Layer);
        return coordinate;
    }
    const int x = m_centerSubfieldCoordinate->subfieldX() + int((StretchedPixelCoordinate.x() - m_size.width() * 0.5) / m_scaleFactor);
    const int y = m_centerSubfieldCoordinate->subfieldY() + int((StretchedPixelCoordinate.y() - m_size.height() * 0.5) / m_scaleFactor);
    coordinate->setSubfieldX(x);
    coordinate->setSubfieldY(y);
    coordinate->setZ(Layer);
    return coordinate;
}

QPoint TWorldMapCameraViewport::convertSubfieldCoordinateToStretchedPixelCoordinate(
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate)
{
    if (!WorldMapSubfieldCoordinate || m_scaleFactor <= 0)
        return {};
    const int dx = WorldMapSubfieldCoordinate->subfieldX() - m_centerSubfieldCoordinate->subfieldX();
    const int dy = WorldMapSubfieldCoordinate->subfieldY() - m_centerSubfieldCoordinate->subfieldY();
    return { int(m_size.width() * 0.5 + dx * m_scaleFactor), int(m_size.height() * 0.5 + dy * m_scaleFactor) };
}

void TWorldMapCameraViewport::centerAtField(const int x, const int y, const int z)
{
    applyCenter(x * SubfieldsPerField + SubfieldsPerField / 2, y * SubfieldsPerField + SubfieldsPerField / 2, z);
}

void TWorldMapCameraViewport::applyCenter(const int subfieldX, const int subfieldY, const int z)
{
    m_centerSubfieldCoordinate->setSubfieldX(subfieldX);
    m_centerSubfieldCoordinate->setSubfieldY(subfieldY);
    m_centerSubfieldCoordinate->setZ(z);
    m_topLayer = std::max(m_topLayer, z);
    m_bottomLayer = std::min(m_bottomLayer, z);
    if (m_topLayer < z)
        m_topLayer = z;
    if (m_bottomLayer > z)
        m_bottomLayer = z;
    rebuildCorners();
    Q_EMIT viewportChanged();
}

void TWorldMapCameraViewport::rebuildCorners()
{
    const int halfW = m_scaleFactor > 0 && m_size.width() > 0 ? int(m_size.width() * 0.5 / m_scaleFactor) : 0;
    const int halfH = m_scaleFactor > 0 && m_size.height() > 0 ? int(m_size.height() * 0.5 / m_scaleFactor) : 0;
    const int cx = m_centerSubfieldCoordinate->subfieldX();
    const int cy = m_centerSubfieldCoordinate->subfieldY();
    const int z = m_centerSubfieldCoordinate->z();
    m_centerLayerTopLeftSubfieldCoordinate->setSubfieldX(cx - halfW);
    m_centerLayerTopLeftSubfieldCoordinate->setSubfieldY(cy - halfH);
    m_centerLayerTopLeftSubfieldCoordinate->setZ(z);
    m_centerLayerBottomRightSubfieldCoordinate->setSubfieldX(cx + halfW);
    m_centerLayerBottomRightSubfieldCoordinate->setSubfieldY(cy + halfH);
    m_centerLayerBottomRightSubfieldCoordinate->setZ(z);
    rebuildVolume();
}

void TWorldMapCameraViewport::rebuildVolume()
{
    if (!m_cuboid)
        return;
    constexpr int SubfieldsPerField = 32;
    const int topLeftX = m_centerLayerTopLeftSubfieldCoordinate->subfieldX() / SubfieldsPerField;
    const int topLeftY = m_centerLayerTopLeftSubfieldCoordinate->subfieldY() / SubfieldsPerField;
    const int bottomRightX = m_centerLayerBottomRightSubfieldCoordinate->subfieldX() / SubfieldsPerField;
    const int bottomRightY = m_centerLayerBottomRightSubfieldCoordinate->subfieldY() / SubfieldsPerField;
    m_cuboid->setFromFieldBox(topLeftX, topLeftY, m_topLayer, bottomRightX, bottomRightY, m_bottomLayer);
}

}
