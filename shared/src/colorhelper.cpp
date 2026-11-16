#include "colorhelper.h"

namespace tibia::shared {

namespace {

constexpr int OutfitColumns = 19;
constexpr int OutfitRows = 7;
constexpr int LastOutfitColor = 133;
constexpr float OutfitSaturation[OutfitRows] = { 0.25f, 0.25f, 0.5f, 0.667f, 1.0f, 1.0f, 1.0f };
constexpr float OutfitValue[OutfitRows] = { 1.0f, 0.75f, 0.75f, 0.75f, 1.0f, 0.75f, 0.5f };

}

QColor colorFromEightBit(const std::uint8_t index)
{
    if (index >= 216)
        return QColor::fromRgb(0, 0, 0, 255);
    return QColor::fromRgb(index / 36 * 51, index / 6 % 6 * 51, index % 6 * 51, 255);
}

std::uint8_t eightBitFromColor(const QColor& color)
{
    return std::uint8_t(color.red() / 51 * 36 + color.green() / 51 * 6 + color.blue() / 51);
}

QColor colorFromHsv(const float hue, const float saturation, const float value)
{
    return QColor::fromRgba(QColor::fromHsvF(hue, saturation, value, 1.0f).rgba());
}

QColor outfitColor(const int index)
{
    if (index > LastOutfitColor)
        return QColor(Qt::white);
    const int column = index % OutfitColumns;
    const int row = index / OutfitColumns;
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    if (column == 0) {
        value = 1.0f - float(row) / 7.0f;
    } else {
        hue = float(column) * (1.0f / 18.0f);
        if (row < OutfitRows) {
            saturation = OutfitSaturation[row];
            value = OutfitValue[row];
        } else {
            saturation = 1.0f;
            value = 1.0f;
        }
    }
    return QColor::fromRgba(QColor::fromHsvF(hue, saturation, value, 1.0f).rgba());
}

std::uint8_t outfitColorIndex(const QColor& color)
{
    for (int index = 0; index <= LastOutfitColor; ++index) {
        if (outfitColor(index) == color)
            return std::uint8_t(index);
    }
    return 0;
}

}
