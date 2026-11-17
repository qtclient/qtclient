#pragma once

#include "worldmap/coordinates.h"

#include <QColor>
#include <QImage>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace tibia {
namespace worldmap {

/// The light of one frame, one colour per field of the visible area (CIP: TLightMap).
///
/// The grid is sheared: a field one floor below the origin's floor lands one
/// field down-right, the way floors are drawn. Next to the colour grid the map
/// holds a cloud-shadow grid, eight fields wider and higher, that darkens open
/// fields under the sky while clouds pass.
class TLightMap
{
public:
    /// The point light a field shows (CIP: the light record's info part).
    struct LightSource
    {
        TWorldMapSubfieldCoordinate position;
        uint8_t intensity = 0;
        QColor color;
    };
    /// Whether a field holds a bank; point lights reach a field without one dimmed.
    using FieldPredicate = std::function<bool(const TWorldMapCoordinate&)>;

    /// `ambientLevel` brightens the darkness (0..1); `separator` dims the light of the
    /// floors below the camera's; `cloudsIndoor` darkens fields under a roof and is the
    /// depth of the cloud shadows; `aboveGround` picks the level whose colour fills the
    /// map; `timestampMs` moves the clouds.
    TLightMap(float ambientLevel, float separator, float cloudsIndoor, float cloudsIndoorScale,
              const TWorldMapCoordinate& origin, int width, int height, const QColor& ambientColor,
              bool aboveGround, qint64 timestampMs);
    virtual ~TLightMap() = default;

    /// The field at the grid's top-left, on the floor the grid is built for.
    TWorldMapCoordinate origin() const { return m_origin; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    /// The colour cell of a field; null outside the grid.
    QColor* cellAt(const TWorldMapCoordinate& coordinate);
    const QColor* cellAt(const TWorldMapCoordinate& coordinate) const;
    /// The cloud factor of a field, 0 outside the cloud grid.
    float cloudFactorAt(const TWorldMapCoordinate& coordinate) const;
    /// QColor::lightness of a field's light, 0 outside the grid.
    int lightnessAt(const TWorldMapCoordinate& coordinate) const;
    /// Lightness at a subfield, bilinear between the four fields around it, 0..255.
    int lightnessAt(const TWorldMapSubfieldCoordinate& coordinate) const;
    /// A field's light; white outside the grid.
    QColor colorAt(const TWorldMapCoordinate& coordinate) const;
    /// Sets a field's light; ignored outside the grid.
    void setColorAt(const TWorldMapCoordinate& coordinate, const QColor& color);
    /// Adds a point light: every field closer than its intensity (in fields) takes the
    /// brighter of its own colour and the light's, falling off over five fields.
    void addLightSource(const TWorldMapSubfieldCoordinate& position, const LightSource& light,
                        const FieldPredicate& hasBank);
    /// Dims every field by the separator (the floors below the camera are done).
    void applyFloorSeparator();
    /// The base colour of a level: 0 is the darkness below ground, 1 the ambient light.
    QColor levelColor(int level) const;
    /// Lights a field with its level's colour (the map's own level).
    void applyLevelColor(const TWorldMapCoordinate& coordinate, bool underOpenSky);
    /// Lights a field with a level's colour, darkened indoors or by the clouds.
    void applyLevelColor(const TWorldMapCoordinate& coordinate, uint8_t level, bool underOpenSky);
    /// The factor a level colour is darkened by: indoors the squared indoor factor,
    /// under the sky the cloud shadow.
    float levelColorScale(const TWorldMapCoordinate& coordinate, bool underOpenSky) const;
    /// Whether fields under a roof are darkened at all.
    bool darkensIndoors() const { return m_cloudsIndoorScale != 1.f; }
    /// The grid as an image, one pixel per field.
    QImage toImage() const;
    /// Each row's first field takes its neighbour's light.
    void copySecondColumnToFirst();
    /// The first row takes the second row's light.
    void copySecondRowToFirst();

private:
    int gridIndex(const TWorldMapCoordinate& coordinate) const;
    void computeClouds(qint64 timestampMs);

    TWorldMapCoordinate m_origin;
    TWorldMapSubfieldCoordinate m_subfieldOrigin;
    int m_width = 0;
    int m_height = 0;
    int m_cloudWidth = 0;
    int m_cloudHeight = 0;
    int m_cloudOriginX = 0;
    int m_cloudOriginY = 0;
    uint8_t m_intensity = 0;
    QColor m_lightColor;
    float m_separator = 1.f;
    float m_cloudsIndoor = 1.f;
    float m_cloudsIndoorScale = 1.f;
    QColor m_ambientColor;
    std::map<uint8_t, QColor> m_levelColors;
    uint8_t m_level = 0;
    std::vector<QColor> m_grid;
    std::vector<float> m_clouds;
};

}
}
