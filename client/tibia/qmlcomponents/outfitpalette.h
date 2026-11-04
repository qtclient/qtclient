#pragma once

#include <QColor>

namespace tibia::qmlcomponents {

/// The 133 outfit colours (19 hues x 7 saturation/value steps; CIP 0x10094df40).
/// Pack-safe leaf — no QML types / cip_gen hubs.
namespace outfitpalette {
constexpr int HueSteps = 19;
constexpr int Count = HueSteps * 7;
QColor color(int index);
/// The palette index of `color`, 0 when it is not a palette colour.
int index(const QColor& color);
}

} // namespace tibia::qmlcomponents
