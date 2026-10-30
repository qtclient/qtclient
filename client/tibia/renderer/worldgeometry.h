#pragma once

#include <client/position.h>

#include <QImage>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tibia::qmlcomponents {

/// Shared roof / outdoor gather for classic light, raycast sun, and climate.
/// Independent of which lighting *look* is active.
struct WorldGeometryInput
{
    Position camera;
    int drawWidth = 0;
    int drawHeight = 0;
    int centerX = 0;
    int centerY = 0;
    /// Inclusive visible z range (lowest z .. highest z), same as the world provider.
    int firstFloor = 0;
    int lastFloor = 0;
    /// Roof marks skip banks below this (surface: lowest visible; underground: camera).
    int heightOriginZ = 0;
};

struct WorldGeometry
{
    static constexpr int FieldSize = 32;

    int width = 0;
    int height = 0;
    /// Per field (y * width + x): ceiling floors above the tile (0 = open sky, 1–15).
    std::vector<uint8_t> ceilingFloors;
    /// Per field: outdoor impact height 0–255 (walls / elevation). Indoor unused (mask cuts rain).
    std::vector<uint8_t> surfaceHeight;
    /// Per field: dominant outdoor ThingShape (0 = none; else Flat=1 … Ridge=6). Auto never stored.
    std::vector<uint8_t> surfaceShape;
    /// Grayscale outdoor mask: 255 when topmost drawable in the cell is outdoor
    /// (incl. visible upper decks), 0 when that owner is under roof (1 px/field).
    QImage outdoorMask;
    /// Outdoor: R = surfaceHeight (wall/object tops for rain collide + splash).
    /// Indoor: R = ceilingFloors / 15 * 255. Same size as outdoorMask.
    QImage ceilingMap;
    /// Outdoor: R = ThingShape byte (same size as outdoorMask). Indoor: 0.
    QImage shapeMap;

    bool valid() const { return width > 0 && height > 0 && !outdoorMask.isNull(); }
    uint8_t ceilingAt(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height || ceilingFloors.empty())
            return 0;
        return ceilingFloors[static_cast<size_t>(y * width + x)];
    }
    /// True when rain paints this FOV cell (`outdoorMask` open) — same check as precip.
    bool outdoorAt(int x, int y) const
    {
        if (outdoorMask.isNull() || x < 0 || y < 0 || x >= outdoorMask.width() || y >= outdoorMask.height())
            return false;
        return outdoorMask.constScanLine(y)[x] > 0;
    }

    /// Fraction of FOV fields where rain paints (`outdoorMask` open), 0–1.
    /// Debug / overlay mood only — weather beds use `outdoorAt` at the camera cell.
    float outdoorFraction() const
    {
        if (outdoorMask.isNull() || outdoorMask.width() <= 0 || outdoorMask.height() <= 0)
            return 0.f;
        const int w = outdoorMask.width();
        const int h = outdoorMask.height();
        size_t open = 0;
        const size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
        for (int y = 0; y < h; ++y) {
            const uchar* row = outdoorMask.constScanLine(y);
            for (int x = 0; x < w; ++x) {
                if (row[x] > 0)
                    ++open;
            }
        }
        return total > 0 ? static_cast<float>(open) / static_cast<float>(total) : 0.f;
    }
};

/// Walk visible floors with CIP `underRoofFloors`; max cover per shared screen cell.
WorldGeometry buildWorldGeometry(const WorldGeometryInput& in);

/// outdoorMask / ceilingMap / shapeMap from ceilingFloors + surface height/shape.
/// Weather rewrites outdoorMask for open-sky + outdoor upper faces (see buildWorldGeometry).
void fillGeometryImages(WorldGeometry& geo);

}
