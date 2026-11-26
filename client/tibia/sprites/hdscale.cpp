#include "hdscale.h"

#include <cstring>

namespace tibia::sprites {
namespace {

int g_factor = 1;
HdScaleFn g_fn = nullptr;

} // namespace

void setHdScaler(const int factor, const HdScaleFn fn)
{
    // Exclusive hook: first non-null wins; log conflicts (include/qtclient/README.md).
    // nullptr clears the scaler (factor resets to 1).
    if (fn && g_fn && g_fn != fn) {
        return;
    }
    g_factor = factor < 1 ? 1 : factor;
    g_fn = fn;
    if (!g_fn)
        g_factor = 1;
}

int hdFactor()
{
    return g_fn ? g_factor : 1;
}

QImage scaleHd(const QImage& src)
{
    if (src.isNull() || !g_fn || g_factor <= 1)
        return {};
    return g_fn(src.format() == QImage::Format_RGBA8888 ? src : src.convertToFormat(QImage::Format_RGBA8888));
}

QImage scaleNearest(const QImage& source, const int factor)
{
    if (source.isNull() || factor < 1)
        return {};
    const QImage src = source.format() == QImage::Format_RGBA8888 ? source : source.convertToFormat(QImage::Format_RGBA8888);
    if (factor == 1)
        return src;
    const int w = src.width();
    const int h = src.height();
    QImage out(w * factor, h * factor, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = src.constScanLine(y);
        for (int dy = 0; dy < factor; ++dy) {
            uint8_t* dst = out.scanLine(y * factor + dy);
            for (int x = 0; x < w; ++x) {
                const uint8_t* px = row + x * 4;
                const bool transparent = px[3] == 0;
                for (int dx = 0; dx < factor; ++dx) {
                    uint8_t* d = dst + (x * factor + dx) * 4;
                    if (transparent)
                        std::memset(d, 0, 4);
                    else
                        std::memcpy(d, px, 4);
                }
            }
        }
    }
    return out;
}

} // namespace tibia::sprites
