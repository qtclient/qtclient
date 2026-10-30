#pragma once

#include <client/declarations.h>
#include <client/position.h>

#include <QImage>
#include <QPoint>
#include <QRgb>

#include <cstdint>
#include <vector>

class Tile;

namespace tibia::qmlcomponents::light {

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

FieldFlags fieldFlags(Tile& tile);

/// CIP roof mask: a floor above `pos` with `flags.roof` (bank / border / roof-edge,
/// not translucent). Returns floors up to that cover, or 0 if open sky.
uint8_t underRoofFloors(const Position& pos);

struct LightRgb
{
    float r = 0.f, g = 0.f, b = 0.f;
    LightRgb operator*(float k) const { return { r * k, g * k, b * k }; }
    static LightRgb max(const LightRgb& a, const LightRgb& b);
};

struct PointLight
{
    QPoint center;
    uint8_t intensity = 0;
    uint8_t color = 0;
    uint8_t z = 0;
};

struct BuildInput
{
    Position camera;
    int drawWidth = 0;
    int drawHeight = 0;
    int centerX = 0;
    int centerY = 0;
    uint8_t firstFloor = 0;
    uint8_t lastFloor = 0;
    LightOptions options;
};

/// One CIP light floor-walk: ambient (no point lights) and classic (ambient + lights).
/// Raycast samples `ambient`; TLightMap / LightMap UI use `classic`. When `needAmbient` is
/// false and lights are empty, only one grid is filled (classic ≡ ambient).
struct ClassicLightBuild
{
    std::vector<LightRgb> ambient;
    std::vector<LightRgb> classic;
};

ClassicLightBuild buildClassicLightPair(const BuildInput& in, const std::vector<PointLight>& lights,
                                        bool needAmbient = true);

/// Per-field ambient only (banks, roofs, clouds, indoor attenuation, separator).
std::vector<LightRgb> buildFieldAmbient(const BuildInput& in);

/// Full classic TLightMap (ambient floors + per-floor point lights), CIP interleaving.
std::vector<LightRgb> buildClassicLight(const BuildInput& in, const std::vector<PointLight>& lights);

QImage toLightImage(const std::vector<LightRgb>& cells, int width, int height);

Position lightField(const Position& camera, int centerX, int centerY, int x, int y, int z);

}
