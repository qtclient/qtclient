#include "worldmap/coordinates.h"

#include <QDebug>

#include <ostream>

namespace tibia::worldmap {

QDebug operator<<(QDebug debug, const TWorldMapSubfieldCoordinate& coordinate)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "[" << coordinate.subfieldX << "=" << coordinate.subfieldX / 32 << "+" << coordinate.subfieldX % 32
                    << ", " << coordinate.subfieldY << "=" << coordinate.subfieldY / 32 << "+" << coordinate.subfieldY % 32
                    << ", " << coordinate.z << "]";
    return debug;
}

QDebug operator<<(QDebug debug, const TWorldMapCuboid& cuboid)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "TWorldMapCuboid([" << cuboid.origin.x << "," << cuboid.origin.y << "," << cuboid.origin.z << "]"
                    << " - [" << cuboid.end.x << ", " << cuboid.end.y << ", " << cuboid.end.z << "]," << " Extent["
                    << cuboid.extent.width << ", " << cuboid.extent.height << ", " << cuboid.extent.depth << "])";
    return debug;
}

QDebug operator<<(QDebug debug, const TShearedWorldMapCuboid& cuboid)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "TShearedWorldMapCuboid(" << static_cast<const TWorldMapCuboid&>(cuboid) << ",Center Layer "
                    << cuboid.shearFloor << ")";
    return debug;
}

std::ostream& operator<<(std::ostream& stream, const TWorldMapCuboid& cuboid)
{
    return stream << "TWorldMapCuboid([" << cuboid.origin.x << "," << cuboid.origin.y << "," << cuboid.origin.z << "]"
                  << " - [" << cuboid.end.x << ", " << cuboid.end.y << ", " << cuboid.end.z << "]," << " Extent["
                  << cuboid.extent.width << ", " << cuboid.extent.height << ", " << cuboid.extent.depth << "])";
}

std::ostream& operator<<(std::ostream& stream, const TWorldMapSubfieldCoordinate& coordinate)
{
    return stream << "[" << coordinate.subfieldX << "=" << coordinate.subfieldX / 32 << "+" << coordinate.subfieldX % 32 << ", "
                  << coordinate.subfieldY << "=" << coordinate.subfieldY / 32 << "+" << coordinate.subfieldY % 32 << ", "
                  << coordinate.z << "]";
}

}
