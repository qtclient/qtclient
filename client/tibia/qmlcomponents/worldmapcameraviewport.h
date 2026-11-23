#pragma once

#include "renderer/worldmapcameraviewport.h"

#include <cip_gen.h>

namespace tibia {
namespace qmlcomponents {

/// A world map camera for QML (the house preview's WorldMapCameraViewport): a
/// view onto its renderer::TWorldMapCameraViewport. Coordinates and the volume
/// read from it are new objects owned by the JavaScript engine.
class TWorldMapCameraViewportQmlType : public cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCameraViewportQmlType(QObject* parent = nullptr);

    renderer::TWorldMapCameraViewport& cameraViewport() { return m_cameraViewport; }
    const renderer::TWorldMapCameraViewport& cameraViewport() const { return m_cameraViewport; }

    float scaleFactor() const override;
    void setScaleFactor(float v) override;
    QSize size() const override;
    void setSize(const QSize& v) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* centerSubfieldCoordinate() const override;
    void setCenterSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* centerLayerTopLeftSubfieldCoordinate() const override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* centerLayerBottomRightSubfieldCoordinate() const override;
    cip::tibia::qmlcomponents::TWorldMapCuboidQmlType* volume() const override;
    int topLayer() const override;
    void setTopLayer(int v) override;
    int bottomLayer() const override;
    void setBottomLayer(int v) override;
    QPoint centerLayerStretchedPixelOffset() const override;

    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* convertToWorldMapSubfieldCoordinate(
        const QPoint& StretchedPixelCoordinate) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* convertToWorldMapSubfieldCoordinateForLayer(
        const QPoint& StretchedPixelCoordinate, int Layer) override;
    /// (-1, -1) without a coordinate.
    QPoint convertCoordinateToStretchedPixelCoordinate(
        cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* WorldMapCoordinate) override;
    /// (-1, -1) without a coordinate.
    QPoint convertSubfieldCoordinateToStretchedPixelCoordinate(
        cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate) override;
    void zoomAtPoint(float ScaleFactor, const QPoint& StretchedPixelCoordinate) override;
    void moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(
        cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate,
        const QPoint& PixelCoordinate) override;
    void translateToLayer(int NewLayer) override;
    void setTopAndBottomLayer(int TopLayer, int BottomLayer) override;
    void setAdditionalColumnsAndRows(int AdditionalColumsLeft, int AdditionalColumsRight, int AdditionalRowsTop,
                                     int AdditionalRowsBottom) override;
    void translateByStretchedPixelCoordinate(const QPoint& StretchedPixelCoordinate) override;
    void translateBySubfieldOffset(const QPoint& SubfieldOffset) override;

private:
    renderer::TWorldMapCameraViewport m_cameraViewport;
};

}
}
