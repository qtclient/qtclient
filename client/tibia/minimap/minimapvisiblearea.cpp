#include "minimapvisiblearea.h"

#include "config/sidebaroptions.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace tibia::minimap {

namespace {

constexpr int MaxCoordinate = 0xffff;
constexpr int MaxFloor = 15;

int roundToInt(const double v)
{
    return static_cast<int>(std::clamp(std::round(v), double(INT_MIN), double(INT_MAX)));
}

}

TMinimapVisibleArea::TMinimapVisibleArea(QObject* parent)
    : cip::tibia::minimap::TMinimapVisibleArea(parent)
{
    resizeKeepingCenter();
}

void TMinimapVisibleArea::setOptionsSource(config::TSidebarOptions* options)
{
    disconnect(m_optionsConnection);
    m_options = options;
    if (!options)
        return;
    m_optionsConnection = connect(options, &config::TSidebarOptions::sidebarPanelsOptionsChanged, this,
                                  &TMinimapVisibleArea::restoreZoomLevelFromOptions);
    restoreZoomLevelFromOptions();
}

Position TMinimapVisibleArea::center() const
{
    return Position(m_topLeft.x + m_size.width() / 2, m_topLeft.y + m_size.height() / 2, m_topLeft.z);
}

void TMinimapVisibleArea::setCenter(const Position& center)
{
    m_topLeft = Position(center.x - m_size.width() / 2, center.y - m_size.height() / 2, center.z);
    updateMatrix();
}

void TMinimapVisibleArea::setViewportSizePx(const int width, const int height)
{
    m_viewportPx = QSize(width, height);
    resizeKeepingCenter();
}

bool TMinimapVisibleArea::intersectsTile(const Position& id) const
{
    constexpr int TileSize = 256;
    return id.z == m_topLeft.z
           && m_topLeft.x < id.x + TileSize && id.x < m_topLeft.x + m_size.width()
           && m_topLeft.y < id.y + TileSize && id.y < m_topLeft.y + m_size.height();
}

void TMinimapVisibleArea::moveX(const int dx)
{
    m_topLeft.x = std::max(0, std::min(m_topLeft.x + dx, MaxCoordinate - m_size.width()));
    updateMatrix();
}

void TMinimapVisibleArea::moveY(const int dy)
{
    m_topLeft.y = std::max(0, std::min(m_topLeft.y + dy, MaxCoordinate - m_size.height()));
    updateMatrix();
}

void TMinimapVisibleArea::moveZ(const int dz)
{
    setZ(m_topLeft.z + dz);
}

void TMinimapVisibleArea::setZ(const int z)
{
    m_topLeft.z = static_cast<uint8_t>(std::clamp(z, 0, MaxFloor));
    updateMatrix();
}

void TMinimapVisibleArea::zoomIn()
{
    m_zoom = std::min(m_zoom + 1, MaxZoom);
    resizeKeepingCenter();
    storeZoom();
}

void TMinimapVisibleArea::zoomOut()
{
    m_zoom = std::max(m_zoom - 1, MinZoom);
    resizeKeepingCenter();
    storeZoom();
}

float TMinimapVisibleArea::scale() const
{
    return std::exp2(m_zoom);
}

Position TMinimapVisibleArea::itemToWorld(const QPoint& itemPixel) const
{
    const QPointF world = m_matrix.inverted().map(QPointF(itemPixel));
    return Position(static_cast<int>(std::floor(world.x())), static_cast<int>(std::floor(world.y())), m_topLeft.z);
}

QPoint TMinimapVisibleArea::worldToItem(const int x, const int y) const
{
    const QPointF item = m_matrix.map(QPointF(x + 0.5f, y + 0.5f));
    return QPoint(roundToInt(item.x()), roundToInt(item.y()));
}

void TMinimapVisibleArea::restoreZoomLevelFromOptions()
{
    if (!m_options)
        return;
    m_zoom = m_options->sidebarPanelsOptions().minimapZoomLevel;
    resizeKeepingCenter();
}

void TMinimapVisibleArea::updateMatrix()
{
    // CIP snaps the top-left to whole item pixels.
    const float s = scale();
    m_matrix.setToIdentity();
    m_matrix.scale(s);
    m_matrix.translate(-std::floor(m_topLeft.x * s) / s, -std::floor(m_topLeft.y * s) / s);
}

void TMinimapVisibleArea::resizeKeepingCenter()
{
    const Position keep = center();
    const float s = scale();
    m_size = QSize(static_cast<int>(std::round(m_viewportPx.width() / s)), static_cast<int>(std::round(m_viewportPx.height() / s)));
    setCenter(keep);
}

void TMinimapVisibleArea::storeZoom()
{
    if (!m_options)
        return;
    config::SidebarPanelsOptions options = m_options->sidebarPanelsOptions();
    options.minimapZoomLevel = m_zoom;
    m_options->setSidebarPanelsOptions(options);
}

}
