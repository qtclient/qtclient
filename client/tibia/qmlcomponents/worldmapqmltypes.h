#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

namespace tibia {
namespace qmlcomponents {

/// Field coordinate exposed to QML (CIP: TWorldMapCoordinateQmlType).
class TWorldMapCoordinateQmlType : public cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCoordinateQmlType(QObject* parent = nullptr);
    explicit TWorldMapCoordinateQmlType(const worldmap::TWorldMapCoordinate& coordinate, QObject* parent = nullptr);

    /// A new coordinate owned by the JavaScript engine (handed to QML).
    static TWorldMapCoordinateQmlType* create(const worldmap::TWorldMapCoordinate& coordinate);

    bool equals(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther) override;
    int distanceTo(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther) override;
};

/// Sub-field (pixel) coordinate exposed to QML (CIP: TWorldMapSubfieldCoordinateQmlType).
class TWorldMapSubfieldCoordinateQmlType : public cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapSubfieldCoordinateQmlType(QObject* parent = nullptr);
    explicit TWorldMapSubfieldCoordinateQmlType(const worldmap::TWorldMapSubfieldCoordinate& coordinate,
                                                QObject* parent = nullptr);

    /// A new coordinate owned by the JavaScript engine (handed to QML).
    static TWorldMapSubfieldCoordinateQmlType* create(const worldmap::TWorldMapSubfieldCoordinate& coordinate);

    worldmap::TWorldMapSubfieldCoordinate coordinate() const { return { subfieldX(), subfieldY(), z() }; }

    bool equals(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pOther) override;
};

/// A box of fields handed to QML (CIP: TWorldMapCuboidQmlType). Every read of a
/// corner or the extent hands out a new coordinate owned by the JavaScript engine.
class TWorldMapCuboidQmlType : public cip::tibia::qmlcomponents::TWorldMapCuboidQmlType
{
    Q_OBJECT
public:
    explicit TWorldMapCuboidQmlType(QObject* parent = nullptr);
    explicit TWorldMapCuboidQmlType(const worldmap::TWorldMapCuboid& cuboid, QObject* parent = nullptr);

    /// A new box owned by the JavaScript engine (handed to QML).
    static TWorldMapCuboidQmlType* create(const worldmap::TWorldMapCuboid& cuboid);

    /// The origin.
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* highestTopLeftCoordinate() const override;
    /// One past the last field on every axis.
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* lowestBottomRightCoordinate() const override;
    /// Width, height and depth as x, y and z.
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* extent() const override;

    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType*
    calculateWorldMapCoordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(int index) override;

private:
    worldmap::TWorldMapCuboid m_cuboid;
};

}
}
