#include "qmlcomponents/worldmapcameraviewport.h"

#include "qmlcomponents/worldmapqmltypes.h"

namespace tibia::qmlcomponents {

namespace {

worldmap::TWorldMapSubfieldCoordinate subfieldCoordinateOf(const cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType& coordinate)
{
    return { coordinate.subfieldX(), coordinate.subfieldY(), coordinate.z() };
}

}

TWorldMapCameraViewportQmlType::TWorldMapCameraViewportQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType(parent)
    , m_cameraViewport(nullptr)
{
    connect(&m_cameraViewport, &renderer::TWorldMapCameraViewport::viewportChanged, this,
            &TWorldMapCameraViewportQmlType::viewportChanged);
}

float TWorldMapCameraViewportQmlType::scaleFactor() const
{
    return m_cameraViewport.scaleFactor();
}

void TWorldMapCameraViewportQmlType::setScaleFactor(const float v)
{
    m_cameraViewport.setScaleFactor(v);
}

QSize TWorldMapCameraViewportQmlType::size() const
{
    return m_cameraViewport.size();
}

void TWorldMapCameraViewportQmlType::setSize(const QSize& v)
{
    m_cameraViewport.setSize(v);
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* TWorldMapCameraViewportQmlType::centerSubfieldCoordinate() const
{
    return TWorldMapSubfieldCoordinateQmlType::create(m_cameraViewport.centerSubfieldCoordinate());
}

void TWorldMapCameraViewportQmlType::setCenterSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v)
{
    // Host: QML may assign null.
    if (!v)
        return;
    m_cameraViewport.setCenterSubfieldCoordinate(subfieldCoordinateOf(*v));
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType*
TWorldMapCameraViewportQmlType::centerLayerTopLeftSubfieldCoordinate() const
{
    return TWorldMapSubfieldCoordinateQmlType::create(m_cameraViewport.centerLayerTopLeftSubfieldCoordinate());
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType*
TWorldMapCameraViewportQmlType::centerLayerBottomRightSubfieldCoordinate() const
{
    return TWorldMapSubfieldCoordinateQmlType::create(m_cameraViewport.centerLayerBottomRightSubfieldCoordinate());
}

cip::tibia::qmlcomponents::TWorldMapCuboidQmlType* TWorldMapCameraViewportQmlType::volume() const
{
    return TWorldMapCuboidQmlType::create(m_cameraViewport.volume().cuboid());
}

int TWorldMapCameraViewportQmlType::topLayer() const
{
    return m_cameraViewport.topLayer();
}

void TWorldMapCameraViewportQmlType::setTopLayer(const int v)
{
    m_cameraViewport.setTopAndBottomLayer(v, m_cameraViewport.bottomLayer());
}

int TWorldMapCameraViewportQmlType::bottomLayer() const
{
    return m_cameraViewport.bottomLayer();
}

void TWorldMapCameraViewportQmlType::setBottomLayer(const int v)
{
    m_cameraViewport.setTopAndBottomLayer(m_cameraViewport.topLayer(), v);
}

QPoint TWorldMapCameraViewportQmlType::centerLayerStretchedPixelOffset() const
{
    return m_cameraViewport.centerLayerStretchedPixelOffset();
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType*
TWorldMapCameraViewportQmlType::convertToWorldMapSubfieldCoordinate(const QPoint& StretchedPixelCoordinate)
{
    return TWorldMapSubfieldCoordinateQmlType::create(m_cameraViewport.convertToWorldMapSubfieldCoordinate(StretchedPixelCoordinate));
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType*
TWorldMapCameraViewportQmlType::convertToWorldMapSubfieldCoordinateForLayer(const QPoint& StretchedPixelCoordinate, const int Layer)
{
    return TWorldMapSubfieldCoordinateQmlType::create(
        m_cameraViewport.convertToWorldMapSubfieldCoordinateForLayer(StretchedPixelCoordinate, Layer));
}

QPoint TWorldMapCameraViewportQmlType::convertCoordinateToStretchedPixelCoordinate(
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* WorldMapCoordinate)
{
    if (!WorldMapCoordinate)
        return { -1, -1 };
    const worldmap::TWorldMapCoordinate field(WorldMapCoordinate->x(), WorldMapCoordinate->y(), WorldMapCoordinate->z());
    return m_cameraViewport.convertSubfieldCoordinateToStretchedPixelCoordinate(
        worldmap::TWorldMapSubfieldCoordinate::fromField(field, 0, 0));
}

QPoint TWorldMapCameraViewportQmlType::convertSubfieldCoordinateToStretchedPixelCoordinate(
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate)
{
    if (!WorldMapSubfieldCoordinate)
        return { -1, -1 };
    return m_cameraViewport.convertSubfieldCoordinateToStretchedPixelCoordinate(subfieldCoordinateOf(*WorldMapSubfieldCoordinate));
}

void TWorldMapCameraViewportQmlType::zoomAtPoint(const float ScaleFactor, const QPoint& StretchedPixelCoordinate)
{
    m_cameraViewport.zoomAtPoint(ScaleFactor, StretchedPixelCoordinate);
}

void TWorldMapCameraViewportQmlType::moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate, const QPoint& PixelCoordinate)
{
    if (!WorldMapSubfieldCoordinate)
        return;
    m_cameraViewport.moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(subfieldCoordinateOf(*WorldMapSubfieldCoordinate),
                                                                              PixelCoordinate);
}

void TWorldMapCameraViewportQmlType::translateToLayer(const int NewLayer)
{
    m_cameraViewport.translateToLayer(NewLayer);
}

void TWorldMapCameraViewportQmlType::setTopAndBottomLayer(const int TopLayer, const int BottomLayer)
{
    m_cameraViewport.setTopAndBottomLayer(TopLayer, BottomLayer);
}

void TWorldMapCameraViewportQmlType::setAdditionalColumnsAndRows(const int AdditionalColumsLeft, const int AdditionalColumsRight,
                                                                 const int AdditionalRowsTop, const int AdditionalRowsBottom)
{
    m_cameraViewport.setAdditionalColumnsAndRows(AdditionalColumsLeft, AdditionalColumsRight, AdditionalRowsTop,
                                                 AdditionalRowsBottom);
}

void TWorldMapCameraViewportQmlType::translateByStretchedPixelCoordinate(const QPoint& StretchedPixelCoordinate)
{
    m_cameraViewport.translateByStretchedPixelCoordinate(StretchedPixelCoordinate);
}

void TWorldMapCameraViewportQmlType::translateBySubfieldOffset(const QPoint& SubfieldOffset)
{
    m_cameraViewport.translateBySubfieldOffset(worldmap::TWorldMapSubfieldExtent(SubfieldOffset.x(), SubfieldOffset.y(), 0));
}

}
