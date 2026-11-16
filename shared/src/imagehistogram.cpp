#include "imagehistogram.h"

namespace tibia::shared {

TImageHistogram::TImageHistogram(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            const unsigned r = qRed(pixel);
            const unsigned g = qGreen(pixel);
            const unsigned b = qBlue(pixel);
            ++red[r];
            ++green[g];
            ++blue[b];
            ++luminance[(b * 5 + g * 16 + r * 11) >> 5];
        }
    }
}

}
