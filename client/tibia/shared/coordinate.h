#pragma once

#include <QDebug>

#include <algorithm>
#include <ostream>

namespace tibia {
namespace shared {

/// CIP `shared::TCoordinate`: an x/y/z triple (the base of the world-map
/// coordinate and extent types).
class TCoordinate
{
public:
    int x = 0;
    int y = 0;
    int z = 0;

    TCoordinate() = default;
    TCoordinate(const int x_, const int y_, const int z_ = 0)
        : x(x_)
        , y(y_)
        , z(z_)
    {
    }
    virtual ~TCoordinate() = default;

    TCoordinate operator+(const TCoordinate& other) const { return TCoordinate(x + other.x, y + other.y, z + other.z); }
    TCoordinate operator-(const TCoordinate& other) const { return TCoordinate(x - other.x, y - other.y, z - other.z); }

    bool operator==(const TCoordinate& other) const { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const TCoordinate& other) const { return x != other.x || y != other.y || z != other.z; }
    /// Ordered by x, then y, then z.
    bool operator<(const TCoordinate& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        return z < other.z;
    }

    /// Component-wise minimum.
    static TCoordinate minimum(const TCoordinate& a, const TCoordinate& b)
    {
        return TCoordinate(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
    }
    /// Component-wise maximum.
    static TCoordinate maximum(const TCoordinate& a, const TCoordinate& b)
    {
        return TCoordinate(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
    }
};

/// "[x,y,z]".
inline QDebug operator<<(QDebug debug, const TCoordinate& coordinate)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "[" << coordinate.x << "," << coordinate.y << "," << coordinate.z << "]";
    return debug;
}

/// "[x,y,z]".
inline std::ostream& operator<<(std::ostream& stream, const TCoordinate& coordinate)
{
    return stream << "[" << coordinate.x << "," << coordinate.y << "," << coordinate.z << "]";
}

}
}
