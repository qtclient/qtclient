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

std::ostream& operator<<(std::ostream& stream, const TWorldMapSubfieldCoordinate& coordinate)
{
    return stream << "[" << coordinate.subfieldX << "=" << coordinate.subfieldX / 32 << "+" << coordinate.subfieldX % 32 << ", "
                  << coordinate.subfieldY << "=" << coordinate.subfieldY / 32 << "+" << coordinate.subfieldY % 32 << ", "
                  << coordinate.z << "]";
}

}
