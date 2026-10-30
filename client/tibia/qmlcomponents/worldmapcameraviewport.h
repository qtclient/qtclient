#pragma once

#include "qmlcomponents/worldmapqmltypes.h"

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// House-preview / Cyclopedia WorldMap camera (CIP: TWorldMapCameraViewportQmlType).
/// Owns centre / layer corner subfield coordinates so QML never sees null `.z`.
class TWorldMapCameraViewport : public cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCameraViewport(QObject* parent = nullptr);

    void setScaleFactor(float v) override;
    void setSize(const QSize& v) override;
    void setCenterSubfieldCoordinate(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* v) override;
    void setTopAndBottomLayer(int TopLayer, int BottomLayer) override;
    void translateToLayer(int NewLayer) override;
    void translateByStretchedPixelCoordinate(const QPoint& StretchedPixelCoordinate) override;
    void translateBySubfieldOffset(const QPoint& SubfieldOffset) override;
    void moveWorldMapSubfieldCoordinateToStretchedPixelCoordinate(
        cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate,
        const QPoint& PixelCoordinate) override;
    void zoomAtPoint(float ScaleFactor, const QPoint& StretchedPixelCoordinate) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* convertToWorldMapSubfieldCoordinate(
        const QPoint& StretchedPixelCoordinate) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* convertToWorldMapSubfieldCoordinateForLayer(
        const QPoint& StretchedPixelCoordinate, int Layer) override;
    QPoint convertSubfieldCoordinateToStretchedPixelCoordinate(
        cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* WorldMapSubfieldCoordinate) override;

    /// Centres field (x,y,z) in the viewport (subfield = field * 32 + 16).
    void centerAtField(int x, int y, int z);

private:
    void rebuildCorners();
    void rebuildVolume();
    void applyCenter(int subfieldX, int subfieldY, int z);

    static constexpr int SubfieldsPerField = 32;
    TWorldMapCuboidQmlType* m_cuboid = nullptr;
};

}
