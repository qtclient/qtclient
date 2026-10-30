#pragma once

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// Field coordinate exposed to QML (CIP: TWorldMapCoordinateQmlType).
class TWorldMapCoordinateQmlType : public cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCoordinateQmlType(QObject* parent = nullptr);

    bool equals(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther) override;
    int distanceTo(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther) override;
};

/// Sub-field (pixel) coordinate exposed to QML (CIP: TWorldMapSubfieldCoordinateQmlType).
class TWorldMapSubfieldCoordinateQmlType : public cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapSubfieldCoordinateQmlType(QObject* parent = nullptr);

    bool equals(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pOther) override;
};

/// Axis-aligned field volume for camera / storage (CIP: TWorldMapCuboidQmlType;
/// properties highestTopLeft / lowestBottomRight / extent; index walk v3/v4).
class TWorldMapCuboidQmlType : public cip::tibia::qmlcomponents::TWorldMapCuboidQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCuboidQmlType(QObject* parent = nullptr);

    /// Refresh corners + extent from an inclusive field AABB (z high = sky).
    void setFromFieldBox(int topLeftX, int topLeftY, int topZ, int bottomRightX, int bottomRightY, int bottomZ);

    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType*
    calculateWorldMapCoordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(int index) override;
};

}
