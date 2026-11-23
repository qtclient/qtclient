#include "minimaptile.h"

#include "colorhelper.h"

#include <algorithm>
#include <vector>

namespace tibia::minimap {

namespace {

constexpr uint8_t UnknownColor = 0;
constexpr uint8_t UnknownWaypoint = 0xfe;
constexpr uint8_t BlockedWaypoint = 0xff;
constexpr uint8_t MaxWaypointCost = 250;

/// The colour table of the colour image: the automap palette.
QList<QRgb> colorPalette()
{
    QList<QRgb> palette(256, qRgb(0, 0, 0));
    for (int i = 0; i < 216; ++i)
        palette[i] = TMinimapTile::paletteColor(i).rgb();
    return palette;
}

/// The colour table of the waypoint image: costs are grey levels, 251..253
/// red, unknown magenta and blocked yellow.
QList<QRgb> waypointPalette()
{
    QList<QRgb> palette(256);
    for (int i = 0; i < 251; ++i)
        palette[i] = qRgb(i, i, i);
    palette[251] = palette[252] = palette[253] = QColor(Qt::red).rgb();
    palette[UnknownWaypoint] = QColor(Qt::magenta).rgb();
    palette[BlockedWaypoint] = QColor(Qt::yellow).rgb();
    return palette;
}

QImage blankImage(const QList<QRgb>& palette, const uint8_t fill)
{
    QImage image(TMinimapTile::Size, TMinimapTile::Size, QImage::Format_Indexed8);
    image.setColorTable(palette);
    image.fill(fill);
    return image;
}

/// Unseen fields (colour 0) opaque black, seen fields transparent.
QImage unexploredMaskFrom(const QImage& colors)
{
    QImage mask(colors.size(), QImage::Format_ARGB32_Premultiplied);
    const uchar* bits = colors.constScanLine(0);
    const std::vector<uchar> indices(bits, bits + colors.width() * colors.height());
    const QRgb seen = QColor(0, 0, 0, 0).rgba();
    const QRgb unseen = QColor(0, 0, 0, 255).rgba();
    for (int y = 0; y < mask.height(); ++y)
        for (int x = 0; x < mask.width(); ++x)
            mask.setPixel(x, y, indices[y * colors.width() + x] == 0 ? unseen : seen);
    return mask;
}

}

QColor TMinimapTile::paletteColor(const int index)
{
    if (index < 0 || index > 215)
        return QColor(0, 0, 0);
    return QColor(index / 36 * 51, index / 6 % 6 * 51, index % 6 * 51);
}

uint8_t TMinimapTile::paletteIndex(const QColor& color)
{
    return static_cast<uint8_t>(color.red() / 51 * 36 + color.green() / 51 * 6 + color.blue() / 51);
}

TMinimapTile::TMinimapTile(const Position& id)
    : m_id(id)
{
    static const QList<QRgb> colors = colorPalette();
    static const QList<QRgb> waypoints = waypointPalette();
    m_colors = blankImage(colors, UnknownColor);
    m_waypoints = blankImage(waypoints, UnknownWaypoint);
}

TMinimapTile::TMinimapTile(const Position& id, QImage colors, QImage waypoints)
    : m_id(id)
    , m_colors(std::move(colors))
    , m_waypoints(std::move(waypoints))
    , m_loaded(true)
{}

QColor TMinimapTile::colorAt(const Position& coord) const
{
    const QPoint p = pixelOf(coord);
    return paletteColor(m_colors.pixelIndex(p.x(), p.y()));
}

uint8_t TMinimapTile::waypointAt(const Position& coord) const
{
    const QPoint p = pixelOf(coord);
    return static_cast<uint8_t>(m_waypoints.pixelIndex(p.x(), p.y()));
}

void TMinimapTile::setColorIndex(const Position& coord, const uint8_t colorIndex)
{
    const QPoint p = pixelOf(coord);
    if (m_colors.pixelIndex(p) == colorIndex)
        return;
    m_colors.setPixel(p, colorIndex);
    setChanged();
}

void TMinimapTile::setColor(const Position& coord, const QColor& color)
{
    const QPoint p = pixelOf(coord);
    const uint8_t colorIndex = shared::eightBitFromColor(color);
    if (m_colors.pixelIndex(p) == colorIndex)
        return;
    m_colors.setPixel(p, colorIndex);
    setChanged();
}

QImage TMinimapTile::unexploredMask()
{
    if (m_unexploredMask.isNull())
        m_unexploredMask = unexploredMaskFrom(m_colors);
    return m_unexploredMask;
}

void TMinimapTile::setWaypoint(const Position& coord, const uint8_t cost, const bool blocked)
{
    const QPoint p = pixelOf(coord);
    const uint8_t value = blocked ? BlockedWaypoint : std::min(cost, MaxWaypointCost);
    if (m_waypoints.pixelIndex(p) == value)
        return;
    m_waypoints.setPixel(p, value);
    m_changed = true;
}

void TMinimapTile::setField(const Position& coord, const uint8_t colorIndex, const uint8_t cost, const bool blocked)
{
    setColorIndex(coord, colorIndex);
    setWaypoint(coord, cost, blocked);
}

void TMinimapTile::merge(const TMinimapTile& loaded)
{
    for (int y = 0; y < Size; ++y) {
        uchar* colors = m_colors.scanLine(y);
        uchar* waypoints = m_waypoints.scanLine(y);
        const uchar* loadedColors = loaded.m_colors.constScanLine(y);
        const uchar* loadedWaypoints = loaded.m_waypoints.constScanLine(y);
        for (int x = 0; x < Size; ++x) {
            if (colors[x] == UnknownColor)
                colors[x] = loadedColors[x];
            if (waypoints[x] == UnknownWaypoint)
                waypoints[x] = loadedWaypoints[x];
        }
    }
    m_changed = true;
    m_loaded = true;
}

}
