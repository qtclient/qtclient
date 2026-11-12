#include "qmlcomponents/worldmapqmltypes.h"

#include <QQmlEngine>

#include <cstdlib>

namespace tibia::qmlcomponents {

TWorldMapCoordinateQmlType::TWorldMapCoordinateQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType(parent)
{
}

bool TWorldMapCoordinateQmlType::equals(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther)
{
    return pOther && x() == pOther->x() && y() == pOther->y() && z() == pOther->z();
}

int TWorldMapCoordinateQmlType::distanceTo(cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* pOther)
{
    if (!pOther)
        return 0;
    const int dx = std::abs(x() - pOther->x());
    const int dy = std::abs(y() - pOther->y());
    const int dz = std::abs(z() - pOther->z());
    return dx + dy + dz;
}

TWorldMapSubfieldCoordinateQmlType::TWorldMapSubfieldCoordinateQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType(parent)
{
}

bool TWorldMapSubfieldCoordinateQmlType::equals(cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* pOther)
{
    return pOther && subfieldX() == pOther->subfieldX() && subfieldY() == pOther->subfieldY() && z() == pOther->z();
}

TWorldMapCuboidQmlType::TWorldMapCuboidQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TWorldMapCuboidQmlType(parent)
{
    m_highestTopLeftCoordinate = new TWorldMapCoordinateQmlType(this);
    m_lowestBottomRightCoordinate = new TWorldMapCoordinateQmlType(this);
    m_extent = new TWorldMapCoordinateQmlType(this);
}

void TWorldMapCuboidQmlType::setFromFieldBox(const int topLeftX, const int topLeftY, const int topZ,
                                            const int bottomRightX, const int bottomRightY, const int bottomZ)
{
    m_highestTopLeftCoordinate->setX(topLeftX);
    m_highestTopLeftCoordinate->setY(topLeftY);
    m_highestTopLeftCoordinate->setZ(topZ);
    m_lowestBottomRightCoordinate->setX(bottomRightX);
    m_lowestBottomRightCoordinate->setY(bottomRightY);
    m_lowestBottomRightCoordinate->setZ(bottomZ);
    m_extent->setX(bottomRightX - topLeftX + 1);
    m_extent->setY(bottomRightY - topLeftY + 1);
    m_extent->setZ(topZ - bottomZ + 1);
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType*
TWorldMapCuboidQmlType::calculateWorldMapCoordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(const int index)
{
    const int width = m_extent->x();
    const int height = m_extent->y();
    const int depth = m_extent->z();
    if (width <= 0 || height <= 0 || depth <= 0 || index < 0 || index >= width * height * depth)
        return nullptr;

    // Column-first (x), then y; within a column walk sky→ground (z descending).
    // Shearing: each floor below the top shifts +1,+1 in field space (CIP isometric).
    const int perColumn = height * depth;
    const int x = index / perColumn;
    const int rem = index % perColumn;
    const int y = rem / depth;
    const int skyOffset = rem % depth;
    auto* coordinate = new TWorldMapCoordinateQmlType;
    QQmlEngine::setObjectOwnership(coordinate, QQmlEngine::JavaScriptOwnership);
    coordinate->setX(m_highestTopLeftCoordinate->x() + x + skyOffset);
    coordinate->setY(m_highestTopLeftCoordinate->y() + y + skyOffset);
    coordinate->setZ(m_highestTopLeftCoordinate->z() - skyOffset);
    return coordinate;
}

}
