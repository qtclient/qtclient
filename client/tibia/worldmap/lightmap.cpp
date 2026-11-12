#include "worldmap/lightmap.h"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace tibia::worldmap {

TLightMap TLightMap::build(const qmlcomponents::light::BuildInput& in,
                           const std::vector<qmlcomponents::light::PointLight>& lights)
{
    TLightMap map;
    // Ambient not requested — callers that also need raycast ambient use buildClassicLightPair.
    map.m_image = qmlcomponents::light::toLightImage(qmlcomponents::light::buildClassicLight(in, lights), in.drawWidth,
                                                     in.drawHeight);
    return map;
}

float TLightMap::lightnessAt(const QImage& image, const QPoint& mapPixel)
{
    if (image.isNull())
        return 1.f;
    const float fx = std::clamp(mapPixel.x() / float(FieldSize) - 0.5f, 0.f, float(image.width() - 1));
    const float fy = std::clamp(mapPixel.y() / float(FieldSize) - 0.5f, 0.f, float(image.height() - 1));
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, image.width() - 1), y1 = std::min(y0 + 1, image.height() - 1);
    const float ax = fx - x0, ay = fy - y0;
    const auto value = [&image](const int x, const int y) {
        const QRgb c = image.pixel(x, y);
        return std::max({ qRed(c), qGreen(c), qBlue(c) }) / 255.f;
    };
    const float top = value(x0, y0) * (1 - ax) + value(x1, y0) * ax;
    const float bottom = value(x0, y1) * (1 - ax) + value(x1, y1) * ax;
    return top * (1 - ay) + bottom * ay;
}

int TLightMap::fieldLightness(const QImage& image, const int fieldX, const int fieldY)
{
    if (image.isNull())
        return 255;
    if (fieldX < 0 || fieldY < 0 || fieldX >= image.width() || fieldY >= image.height())
        return 0;
    return QColor(image.pixel(fieldX, fieldY)).lightness();
}

}
