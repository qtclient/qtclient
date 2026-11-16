#pragma once

#include <QColor>

namespace tibia::client {

/// A colour of the 6x6x6 palette the server sends as one byte (text,
/// light); 0 and bytes past the palette are transparent.
inline QColor colorFromEightBit(const int color, const float brightness = 1.0f)
{
    if (color >= 216 || color <= 0)
        return QColor(0, 0, 0, 0);
    return QColor(static_cast<int>((color / 36 % 6 * 51) * brightness), static_cast<int>((color / 6 % 6 * 51) * brightness),
                  static_cast<int>((color % 6 * 51) * brightness));
}

}
