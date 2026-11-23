#pragma once

#include "sprites/spriteidentifier.h"

#include <client/position.h>

namespace tibia {
namespace sprites {

/// An image of one minimap tile drawn at a scale (CIP
/// sprites::TMinimapTileSpritesheetIdentifier).
class TMinimapTileSpritesheetIdentifier final : public ISpriteIdentifier
{
public:
    /// Which of the tile's images.
    enum EImage
    {
        ColorImage = 0,
        UnexploredMaskImage = 1,
    };

    TMinimapTileSpritesheetIdentifier(float scale, const Position& tileId, int image)
        : m_tileId(tileId)
        , m_image(image)
        , m_scale(scale)
    {
    }

    int identifierKind() const override { return MinimapTileIdentifier; }
    std::shared_ptr<ISpriteIdentifier> clone() const override;
    /// Same tile and scale; the image is not compared.
    bool equals(const ISpriteIdentifier& other) const override;
    /// The tile id's hash, then the scale's bits, then the image, combined in order.
    size_t hash() const override;

    const Position& tileId() const { return m_tileId; }
    int image() const { return m_image; }
    float scale() const { return m_scale; }

    /// The hash of a tile id: z * 1000037 ^ y * 1033 ^ x.
    static uint32_t tileIdHash(const Position& tileId);

private:
    Position m_tileId;
    int m_image = ColorImage;
    float m_scale = 1.f;
};

}
}
