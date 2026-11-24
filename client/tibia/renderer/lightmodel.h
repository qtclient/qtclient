#pragma once

#include <client/declarations.h>
#include <client/position.h>

#include <QImage>
#include <QPoint>
#include <QRgb>

#include <cstdint>
#include <vector>

namespace tibia {
namespace worldmap {
class TWorldMapField;
}
}

namespace tibia {
namespace qmlcomponents {
namespace light {

constexpr int FieldSize = 32;

/// CIP light options (TWorldMapRenderProvider::v7 / option ids 0x32–0x34).
struct LightOptions
{
    float ambientLevel = 0.f;
    float separator = 1.f;
    float attenuation = .5f;
    /// CIP Graphics → Light Effects. When false the LightMap is not drawn and
    /// HUD lightness stays full (nameplates are not dimmed).
    bool effectsEnabled = true;
};

LightOptions loadOptions();

/// Tibia's 8-bit palette colours (6³ cube; 215 is white).
QRgb lightColor(uint8_t color, float brightness);

struct PointLight
{
    QPoint center;
    uint8_t intensity = 0;
    uint8_t color = 0;
    uint8_t z = 0;
};

Position lightField(const Position& camera, int centerX, int centerY, int x, int y, int z);

}
}
}
