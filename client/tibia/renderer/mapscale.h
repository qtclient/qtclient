#pragma once

#include <QPoint>
#include <QPointF>
#include <QSizeF>

#include <algorithm>
#include <cmath>

namespace tibia::qmlcomponents {

/// CIP divides the camera translation by the render scale and rounds to whole pixels.
inline QPoint roundedCamera(const QPoint camera, const double renderScale)
{
    const double scale = renderScale > 0 ? renderScale : 1.0;
    return { static_cast<int>(std::lround(camera.x() / scale)), static_cast<int>(std::lround(camera.y() / scale)) };
}

/// Uniform cover scale: the unscaled map fills the pane on both axes (overflow
/// is clipped). The origin matches MapWindowPane's centred ShaderEffectSource,
/// whose size is the ceiled scaled map.
struct MapCoverScale
{
    double scale = 1.0;
    double drawScaleX = 1.0;
    double drawScaleY = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;

    /// The logical map drawn at `scale`, centered in the pane. CIP's scale pass
    /// uses this: the pane clips whatever hangs past its edges.
    static MapCoverScale placed(const double paneW, const double paneH, const double mapW, const double mapH, const double scale)
    {
        MapCoverScale out;
        if (paneW <= 0 || paneH <= 0 || mapW <= 0 || mapH <= 0 || scale <= 0)
            return out;
        out.scale = scale;
        out.drawScaleX = scale;
        out.drawScaleY = scale;
        const double coverW = mapW * scale;
        const double coverH = mapH * scale;
        out.offsetX = (paneW - coverW) * 0.5;
        out.offsetY = (paneH - coverH) * 0.5;
        return out;
    }

    static MapCoverScale forPane(const double paneW, const double paneH, const double mapW, const double mapH)
    {
        MapCoverScale out;
        if (paneW <= 0 || paneH <= 0 || mapW <= 0 || mapH <= 0)
            return out;
        out.scale = std::max(paneW / mapW, paneH / mapH);
        const double coverW = std::ceil(mapW * out.scale);
        const double coverH = std::ceil(mapH * out.scale);
        out.offsetX = (paneW - coverW) * 0.5;
        out.offsetY = (paneH - coverH) * 0.5;
        out.drawScaleX = coverW / mapW;
        out.drawScaleY = coverH / mapH;
        return out;
    }

    QPointF toPane(const double mapX, const double mapY) const
    {
        return { mapX * drawScaleX + offsetX, mapY * drawScaleY + offsetY };
    }

    QPointF toMap(const double paneX, const double paneY) const
    {
        if (scale <= 0)
            return {};
        return { (paneX - offsetX) / scale, (paneY - offsetY) / scale };
    }

    QPointF toPaneRounded(const double mapX, const double mapY) const
    {
        const QPointF p = toPane(mapX, mapY);
        return { static_cast<double>(std::lround(p.x())), static_cast<double>(std::lround(p.y())) };
    }
};

}
