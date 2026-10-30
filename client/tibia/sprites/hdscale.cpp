#include "hdscale.h"

#include <framework/graphics/image.h>

#include <cstring>

namespace tibia::sprites {
namespace {

int g_factor = 1;
HdScaleFn g_fn = nullptr;

} // namespace

void setHdScaler(const int factor, const HdScaleFn fn)
{
    g_factor = factor < 1 ? 1 : factor;
    g_fn = fn;
    if (!g_fn)
        g_factor = 1;
}

int hdFactor()
{
    return g_fn ? g_factor : 1;
}

ImagePtr scaleHd(const ImagePtr& src)
{
    if (!src || !g_fn || g_factor <= 1)
        return src;
    return g_fn(src);
}

ImagePtr scaleNearest(const ImagePtr& src, const int factor)
{
    if (!src || src->getBpp() != 4 || src->getWidth() <= 0 || src->getHeight() <= 0 || factor < 1)
        return nullptr;
    if (factor == 1)
        return src;

    const int w = src->getWidth();
    const int h = src->getHeight();
    auto out = std::make_shared<Image>(Size(w * factor, h * factor));
    const uint8_t* sp = src->getPixelData();
    uint8_t* dp = out->getPixelData();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const uint8_t* px = sp + (static_cast<size_t>(y) * w + x) * 4;
            const bool transparent = px[3] == 0;
            for (int dy = 0; dy < factor; ++dy) {
                for (int dx = 0; dx < factor; ++dx) {
                    uint8_t* d = dp + (static_cast<size_t>(y * factor + dy) * (w * factor) + (x * factor + dx)) * 4;
                    if (transparent)
                        std::memset(d, 0, 4);
                    else
                        std::memcpy(d, px, 4);
                }
            }
        }
    }
    out->setTransparentPixel(src->hasTransparentPixel());
    return out;
}

} // namespace tibia::sprites
