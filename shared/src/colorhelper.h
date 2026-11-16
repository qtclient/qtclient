#pragma once

#include <QColor>

#include <cstdint>

namespace tibia {
namespace shared {

/// CipSoft's shared colour helpers.

/// The 6x6x6 colour cube of the eight-bit palette: red = index / 36,
/// green = index / 6 % 6, blue = index % 6, each step 51. Indices past the
/// cube (216..255) are black.
QColor colorFromEightBit(std::uint8_t index);
/// The cube index of `color`, each channel rounded down to its step.
std::uint8_t eightBitFromColor(const QColor& color);
/// An opaque colour from hue, saturation and value in [0, 1].
QColor colorFromHsv(float hue, float saturation, float value);
/// The outfit palette: 19 columns (column 0 is grey, the others are hues in
/// eighteenths) by 7 saturation/value rows; indices past 133 are white.
QColor outfitColor(int index);
/// The first outfit palette index whose colour equals `color`; 0 if none.
std::uint8_t outfitColorIndex(const QColor& color);

}
}
