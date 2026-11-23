#include "qmlcomponents/worldmapqmltypes.h"

#include <QJSEngine>

#include <cmath>

namespace tibia::qmlcomponents {

TWorldMapCoordinateQmlType::TWorldMapCoordinateQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType(parent)
{
}

TWorldMapCoordinateQmlType::TWorldMapCoordinateQmlType(const worldmap::TWorldMapCoordinate& coordinate, QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType(parent)
{
    m_x = coordinate.x;
    m_y = coordinate.y;
    m_z = coordinate.z;
}

TWorldMapCoordinateQmlType* TWorldMapCoordinateQmlType::create(const worldmap::TWorldMapCoordinate& coordinate)
{
    auto* qmlCoordinate = new TWorldMapCoordinateQmlType(coordinate);
    QJSEngine::setObjectOwnership(qmlCoordinate, QJSEngine::JavaScriptOwnership);
    return qmlCoordinate;
}

bool TWorldMapCoordinateQmlType::equals(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther)
{
    return pOther && x() == pOther->x() && y() == pOther->y() && z() == pOther->z();
}

/// The straight-line distance on the map plane, truncated; the floors do not count.
int TWorldMapCoordinateQmlType::distanceTo(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther)
{
    if (!pOther)
        return 0;
    const double dx = pOther->x() - x();
    const double dy = pOther->y() - y();
    return static_cast<int>(std::sqrt(dx * dx + dy * dy));
}

TWorldMapSubfieldCoordinateQmlType::TWorldMapSubfieldCoordinateQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType(parent)
{
}

TWorldMapSubfieldCoordinateQmlType::TWorldMapSubfieldCoordinateQmlType(const worldmap::TWorldMapSubfieldCoordinate& coordinate,
                                                                       QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType(parent)
{
    m_subfieldX = coordinate.subfieldX;
    m_subfieldY = coordinate.subfieldY;
    m_z = coordinate.z;
}

TWorldMapSubfieldCoordinateQmlType*
TWorldMapSubfieldCoordinateQmlType::create(const worldmap::TWorldMapSubfieldCoordinate& coordinate)
{
    auto* qmlCoordinate = new TWorldMapSubfieldCoordinateQmlType(coordinate);
    QJSEngine::setObjectOwnership(qmlCoordinate, QJSEngine::JavaScriptOwnership);
    return qmlCoordinate;
}

bool TWorldMapSubfieldCoordinateQmlType::equals(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pOther)
{
    return pOther && subfieldX() == pOther->subfieldX() && subfieldY() == pOther->subfieldY() && z() == pOther->z();
}

TWorldMapCuboidQmlType::TWorldMapCuboidQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCuboidQmlType(parent)
{
}

TWorldMapCuboidQmlType::TWorldMapCuboidQmlType(const worldmap::TWorldMapCuboid& cuboid, QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCuboidQmlType(parent)
    , m_cuboid(cuboid)
{
}

TWorldMapCuboidQmlType* TWorldMapCuboidQmlType::create(const worldmap::TWorldMapCuboid& cuboid)
{
    auto* qmlCuboid = new TWorldMapCuboidQmlType(cuboid);
    QJSEngine::setObjectOwnership(qmlCuboid, QJSEngine::JavaScriptOwnership);
    return qmlCuboid;
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TWorldMapCuboidQmlType::highestTopLeftCoordinate() const
{
    return TWorldMapCoordinateQmlType::create(m_cuboid.getOrigin());
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TWorldMapCuboidQmlType::lowestBottomRightCoordinate() const
{
    return TWorldMapCoordinateQmlType::create(m_cuboid.getEnd());
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TWorldMapCuboidQmlType::extent() const
{
    const worldmap::TWorldMapExtent extent = m_cuboid.getExtent();
    return TWorldMapCoordinateQmlType::create(worldmap::TWorldMapCoordinate(extent.width, extent.height, extent.depth));
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType*
TWorldMapCuboidQmlType::calculateWorldMapCoordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(const int index)
{
    // The index is widened with its sign, as the box's own walk takes it.
    return TWorldMapCoordinateQmlType::create(m_cuboid.coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(
        static_cast<std::size_t>(static_cast<long>(index))));
}

}
