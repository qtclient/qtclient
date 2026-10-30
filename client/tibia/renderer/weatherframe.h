#pragma once

#include <QImage>
#include <QPointF>
#include <QSize>

namespace tibia::qmlcomponents {

/// Neutral climate overlay inputs (map-pixel space). Packs fill via WeatherDriver
/// / WorldOverlayFactory; stock only builds this when a weather driver is set.
struct WeatherFrame
{
    bool active = false;
    /// Precip: 0 clear, 1 rain, 2 snow, 3 storm
    int kind = 0;
    float intensity = 0.f;
    float fog = 0.f;
    float clouds = 0.f;
    float windX = 0.f;
    float windY = 0.f;
    float time = 0.f;
    float lightning = 0.f;
    float outdoorFraction = 0.f;
    float fallX = 0.f;
    float fallY = 1.f;
    QPointF worldOrigin;
    QSize mapSize;
    QImage outdoorMask;
    QImage ceilingMap;
    QImage shapeMap;
};

} // namespace tibia::qmlcomponents
