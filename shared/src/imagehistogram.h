#pragma once

#include <QImage>

#include <array>

namespace tibia {
namespace shared {

/// The per-channel histograms of an image: how many pixels have each red,
/// green and blue value, and each luminance (red * 11 + green * 16 +
/// blue * 5) / 32.
struct TImageHistogram
{
    explicit TImageHistogram(const QImage& image);

    std::array<int, 256> red {};
    std::array<int, 256> green {};
    std::array<int, 256> blue {};
    std::array<int, 256> luminance {};
};

}
}
