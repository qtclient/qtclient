#include "worldmap/lightmap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace tibia::worldmap {

namespace {

/// The highest floor above ground; also how far the cloud grid reaches past the map.
constexpr int GroundFloor = 7;
/// The time unit the clouds move in, in milliseconds.
constexpr qint64 CloudTimeUnit = 500;
/// The clouds repeat after this many milliseconds.
constexpr qint64 CloudCycle = CloudTimeUnit * 89311;

struct CloudWave
{
    qint64 period;
    float xFrequency;
    float yFrequency;
    float amplitude;
};

/// Three sine waves over the map; their sum is the cloud shadow.
constexpr std::array<CloudWave, 3> CloudWaves{ {
    { CloudTimeUnit * 31, 0.5f, 1.f, 0.25f },
    { CloudTimeUnit * 43, 0.25f, -0.25f, 0.5f },
    { CloudTimeUnit * 67, 0.125f, 0.25f, 1.f },
} };

}

TLightMap::TLightMap(const float ambientLevel, const float separator, const float cloudsIndoor,
                     const float cloudsIndoorScale, const TWorldMapCoordinate& origin, const int width, const int height,
                     const QColor& ambientColor, const bool aboveGround, const qint64 timestampMs)
    : m_origin(origin)
    , m_subfieldOrigin(origin.x * 32, origin.y * 32, origin.z)
    , m_width(width)
    , m_height(height)
    , m_cloudWidth(width + 1 + GroundFloor)
    , m_cloudHeight(height + 1 + GroundFloor)
    , m_cloudOriginX(origin.x - GroundFloor + origin.z)
    , m_cloudOriginY(origin.y - GroundFloor + origin.z)
    , m_intensity(static_cast<uint8_t>(static_cast<int>(ambientLevel * 127.f)))
    // Moonlight above ground, white below.
    , m_lightColor(aboveGround ? QColor(200, 200, 255) : QColor(Qt::white))
    , m_separator(separator)
    , m_cloudsIndoor(cloudsIndoor)
    , m_cloudsIndoorScale(cloudsIndoorScale * cloudsIndoor)
    , m_ambientColor(ambientColor)
    , m_level(aboveGround ? 1 : 0)
{
    m_grid.assign(static_cast<size_t>(std::max(0, width * height)), levelColor(m_level));
    m_clouds.assign(static_cast<size_t>(std::max(0, m_cloudWidth * m_cloudHeight)),
                    std::numeric_limits<float>::quiet_NaN());
    m_levelColors[0] = levelColor(0);
    m_levelColors[1] = levelColor(1);
    if (m_cloudsIndoor != 1.f)
        computeClouds(timestampMs);
}

void TLightMap::computeClouds(qint64 timestampMs)
{
    timestampMs %= CloudCycle;
    const float t = static_cast<float>(timestampMs);
    const float drift = std::sin(static_cast<float>(timestampMs * 0.001) / 60.f * 0.5f);
    for (int row = 0; row < m_cloudHeight; ++row) {
        for (int column = 0; column < m_cloudWidth; ++column) {
            float shadow = 0.f;
            for (const CloudWave& wave : CloudWaves) {
                const float phase = static_cast<float>(timestampMs % wave.period) / static_cast<float>(wave.period) * 3.14f;
                const float y = static_cast<float>(m_cloudOriginY + row) + drift * 120.f - t / (CloudTimeUnit * 20.f);
                const float x = static_cast<float>(m_cloudOriginX + column)
                    + static_cast<float>(timestampMs * 0.001) * 0.5f - t / (CloudTimeUnit * 1.9f);
                shadow += std::sin(wave.yFrequency * y + wave.xFrequency * x + phase + phase) * wave.amplitude;
            }
            shadow = std::clamp(shadow * 1.5f, -1.f, 0.f);
            m_clouds[static_cast<size_t>(m_cloudWidth * row + column)]
                = (1.f - m_cloudsIndoor) * (shadow + 1.f) + m_cloudsIndoor;
        }
    }
}

int TLightMap::gridIndex(const TWorldMapCoordinate& coordinate) const
{
    const int dz = coordinate.z - m_origin.z;
    const int x = coordinate.x - m_origin.x + dz;
    const int y = coordinate.y - m_origin.y + dz;
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return -1;
    return m_width * y + x;
}

QColor* TLightMap::cellAt(const TWorldMapCoordinate& coordinate)
{
    const int index = gridIndex(coordinate);
    return index < 0 ? nullptr : &m_grid[static_cast<size_t>(index)];
}

const QColor* TLightMap::cellAt(const TWorldMapCoordinate& coordinate) const
{
    const int index = gridIndex(coordinate);
    return index < 0 ? nullptr : &m_grid[static_cast<size_t>(index)];
}

float TLightMap::cloudFactorAt(const TWorldMapCoordinate& coordinate) const
{
    const int x = coordinate.x - m_cloudOriginX;
    const int y = coordinate.y - m_cloudOriginY;
    if (x < 0 || x >= m_cloudWidth || y < 0 || y >= m_cloudHeight)
        return 0.f;
    return m_clouds[static_cast<size_t>(m_cloudWidth * y + x)];
}

int TLightMap::lightnessAt(const TWorldMapCoordinate& coordinate) const
{
    const QColor* cell = cellAt(coordinate);
    return cell ? cell->lightness() : 0;
}

int TLightMap::lightnessAt(const TWorldMapSubfieldCoordinate& coordinate) const
{
    const int fieldX = coordinate.subfieldX / 32;
    const int fieldY = coordinate.subfieldY / 32;
    const float topLeft = static_cast<float>(lightnessAt(TWorldMapCoordinate(fieldX, fieldY, coordinate.z)));
    const float topRight = static_cast<float>(lightnessAt(TWorldMapCoordinate(fieldX + 1, fieldY, coordinate.z)));
    const float bottomLeft = static_cast<float>(lightnessAt(TWorldMapCoordinate(fieldX, fieldY + 1, coordinate.z)));
    const float bottomRight = static_cast<float>(lightnessAt(TWorldMapCoordinate(fieldX + 1, fieldY + 1, coordinate.z)));
    const float fx = static_cast<float>(coordinate.subfieldX % 32) * 0.03125f;
    const float fy = static_cast<float>(coordinate.subfieldY % 32) * 0.03125f;
    return static_cast<int>(bottomRight * fx * fy + topRight * (1.f - fy) * fx + topLeft * (1.f - fx) * (1.f - fy)
                            + (1.f - fx) * fy * bottomLeft)
        & 0xff;
}

QColor TLightMap::colorAt(const TWorldMapCoordinate& coordinate) const
{
    const QColor* cell = cellAt(coordinate);
    return cell ? *cell : QColor(Qt::white);
}

void TLightMap::setColorAt(const TWorldMapCoordinate& coordinate, const QColor& color)
{
    if (QColor* cell = cellAt(coordinate))
        *cell = color;
}

void TLightMap::addLightSource(const TWorldMapSubfieldCoordinate& position, const LightSource& light,
                               const FieldPredicate& hasBank)
{
    const int intensity = light.intensity;
    if (intensity == 0)
        return;
    const int dz = position.z - m_subfieldOrigin.z;
    const int pixelX = position.subfieldX - m_subfieldOrigin.subfieldX + dz;
    const int pixelY = position.subfieldY - m_subfieldOrigin.subfieldY + dz;
    const int fieldX = pixelX / 32;
    const int fieldY = pixelY / 32;
    const int radius = intensity * 32;
    const unsigned radius2 = static_cast<unsigned>(radius * radius);
    const int firstRow = std::max(0, fieldY - intensity);
    const int lastRow = std::min(m_height - 1, fieldY + intensity);
    const int firstColumn = std::max(0, fieldX - intensity);
    const int lastColumn = std::min(m_width - 1, fieldX + intensity);
    for (int row = firstRow; row <= lastRow; ++row) {
        const int dy = row * 32 - pixelY;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const int dx = column * 32 - pixelX;
            const unsigned distance2 = static_cast<unsigned>(dx * dx + dy * dy);
            if (distance2 >= radius2)
                continue;
            const float falloff = ((static_cast<float>(radius) - static_cast<float>(std::sqrt(static_cast<double>(distance2))))
                                   * 0.03125f)
                / 5.f;
            const TWorldMapCoordinate field(m_origin.x + column, m_origin.y + row, position.z);
            const float dim = (hasBank && hasBank(field)) ? 1.f : m_separator;
            const float strength = std::min(1.f, std::max(0.f, falloff)) * dim;
            const QColor lit = QColor::fromRgbF(light.color.redF() * strength, light.color.greenF() * strength,
                                                light.color.blueF() * strength, 1.f);
            QColor& cell = m_grid[static_cast<size_t>(m_width * row + column)];
            cell.setRed(std::max(cell.red(), lit.red()));
            cell.setGreen(std::max(cell.green(), lit.green()));
            cell.setBlue(std::max(cell.blue(), lit.blue()));
        }
    }
}

void TLightMap::applyFloorSeparator()
{
    for (QColor& cell : m_grid)
        cell.setRgbF(cell.redF() * m_separator, cell.greenF() * m_separator, cell.blueF() * m_separator, 1.f);
}

QColor TLightMap::levelColor(const int level) const
{
    QColor color = level == 0 ? QColor(Qt::black) : m_ambientColor;
    // How much the light brightens is measured on the red channel.
    const int brighten = (255 - color.red()) * m_intensity / 255;
    color.setRed(std::min(255, color.red() + m_lightColor.red() * brighten / 255));
    color.setGreen(std::min(255, color.green() + m_lightColor.green() * brighten / 255));
    color.setBlue(std::min(255, color.blue() + m_lightColor.blue() * brighten / 255));
    return color;
}

void TLightMap::applyLevelColor(const TWorldMapCoordinate& coordinate, const bool underOpenSky)
{
    applyLevelColor(coordinate, m_level, underOpenSky);
}

void TLightMap::applyLevelColor(const TWorldMapCoordinate& coordinate, const uint8_t level, const bool underOpenSky)
{
    QColor color = m_levelColors.at(level);
    if (m_cloudsIndoorScale != 1.f && level != 0) {
        const float scale = levelColorScale(coordinate, underOpenSky);
        color.setRedF(color.redF() * scale);
        color.setGreenF(color.greenF() * scale);
        color.setBlueF(color.blueF() * scale);
    }
    setColorAt(coordinate, color);
}

float TLightMap::levelColorScale(const TWorldMapCoordinate& coordinate, const bool underOpenSky) const
{
    if (!underOpenSky)
        return m_cloudsIndoorScale;
    if (m_cloudsIndoor != 1.f)
        return cloudFactorAt(coordinate);
    return 1.f;
}

QImage TLightMap::toImage() const
{
    QImage image(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x)
            image.setPixel(x, y, m_grid[static_cast<size_t>(m_width * y + x)].rgba());
    }
    return image;
}

void TLightMap::copySecondColumnToFirst()
{
    for (int row = 0; row < m_height; ++row) {
        const size_t first = static_cast<size_t>(row) * static_cast<size_t>(m_width);
        if (first + 1 < m_grid.size())
            m_grid[first] = m_grid[first + 1];
    }
}

void TLightMap::copySecondRowToFirst()
{
    for (int column = 0; column < m_width; ++column) {
        const size_t below = static_cast<size_t>(column + m_width);
        if (below < m_grid.size())
            m_grid[static_cast<size_t>(column)] = m_grid[below];
    }
}

}
