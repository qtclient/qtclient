#pragma once

#include "renderer/lightmodel.h"
#include "shared/cip_typed.h"
#include "worldmap/coordinates.h"

#include <QImage>
#include <QPoint>

#include <vector>

namespace tibia::worldmap {

/// Per-frame classic light grid (CIP: TLightMap, filled by TWorldMapRenderProvider::v7
/// at 0x101056cd0; sample v7 at 0x10107ae40, point lights v10 at 0x10107aff0,
/// image export v16).
class TLightMap : public ::tibia::CipTyped
{
public:
    static constexpr int FieldSize = qmlcomponents::light::FieldSize;

    TLightMap() = default;

    /// Build ambient floors + point lights into a one-pixel-per-field image.
    static TLightMap build(const qmlcomponents::light::BuildInput& in,
                           const std::vector<qmlcomponents::light::PointLight>& lights);

    const QImage& image() const { return m_image; }
    void setImage(QImage image) { m_image = std::move(image); }
    bool isNull() const { return m_image.isNull(); }

    /// Bilinear lightness at a map pixel, 0..1 (CIP: TLightMap::v7).
    float lightnessAt(const QPoint& mapPixel) const { return lightnessAt(m_image, mapPixel); }
    float lightnessAt(const TWorldMapSubfieldCoordinate& subfield) const
    {
        return lightnessAt(subfield.toMapPixel());
    }
    static float lightnessAt(const QImage& image, const QPoint& mapPixel);

    /// QColor::lightness of a field cell, 0 outside the grid (CIP: 0x10107ad30).
    int fieldLightness(int fieldX, int fieldY) const { return fieldLightness(m_image, fieldX, fieldY); }
    static int fieldLightness(const QImage& image, int fieldX, int fieldY);

    void clear() { m_image = {}; }

private:
    QImage m_image;
};

}
