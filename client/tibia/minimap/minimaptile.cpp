#include "minimaptile.h"

#include <algorithm>

namespace tibia::minimap {

namespace {

constexpr uint8_t UnknownColor = 0;
constexpr uint8_t UnknownWaypoint = 0xfe;
constexpr uint8_t BlockedWaypoint = 0xff;
constexpr uint8_t MaxWaypointCost = 250;

// CIP: FUN_100ead6c0.
QList<QRgb> colorPalette()
{
    QList<QRgb> palette(256, qRgb(0, 0, 0));
    for (int i = 0; i < 216; ++i)
        palette[i] = TMinimapTile::paletteColor(i).rgb();
    return palette;
}

// CIP: FUN_100ead930. Costs are grey levels; the top indices mark blocked and unknown.
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

void TMinimapTile::setField(const Position& coord, const uint8_t colorIndex, const uint8_t cost, const bool blocked)
{
    const QPoint p = pixelOf(coord);
    m_colors.setPixel(p, colorIndex);
    m_waypoints.setPixel(p, blocked ? BlockedWaypoint : std::min(cost, MaxWaypointCost));
    m_changed = true;
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
