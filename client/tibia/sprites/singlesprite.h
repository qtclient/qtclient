#pragma once

#include "sprites/sprite.h"

namespace tibia {
namespace sprites {

/// A sprite that owns its image (CIP sprites::TSingleSprite).
class TSingleSprite final : public TSprite
{
public:
    TSingleSprite(double scale, const QImage& image)
        : TSprite(image.size(), scale)
        , m_image(image)
    {
    }

    QImage getImage() const override { return m_image; }

private:
    QImage m_image;
};

}
}
