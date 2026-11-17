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

struct FieldFlags
{
    bool bank = false;
    bool bottom = false;
    bool unsight = false;
    bool dontHide = false;
    bool translucent = false;
    bool roof = false;
    bool wall = false;
};

FieldFlags fieldFlags(tibia::worldmap::TWorldMapField& field);

/// CIP roof mask: a floor above `pos` with `flags.roof` (bank / border / roof-edge,
/// not translucent). Returns floors up to that cover, or 0 if open sky.
uint8_t underRoofFloors(const Position& pos);

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
